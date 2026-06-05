import paho.mqtt.client as mqtt
import cv2
import numpy as np
import face_recognition

# 1. Configuración de Red (Apuntando a la IP de tu computador)
BROKER = "172.20.10.5" 
PORT = 1883
TOPIC_IMAGEN = "smarthome/equipo06/camara/evento"
TOPIC_RESULTADO = "smarthome/equipo06/camara/resultado"
TOPIC_ALERTA = "smarthome/equipo06/alerta"

# 2. Carga de Base de Datos Facial (Rutas relativas)
print("Inicializando motor de visión artificial...")
try:
    img_kevin = face_recognition.load_image_file("fotos/Kevin.jpeg")
    encoding_kevin = face_recognition.face_encodings(img_kevin)[0]

    img_lucas = face_recognition.load_image_file("fotos/Lucas.jpeg")
    encoding_lucas = face_recognition.face_encodings(img_lucas)[0]

    rostros_conocidos = [encoding_kevin, encoding_lucas]
    nombres_conocidos = ["Kevin", "Lucas"]
    print("Vectores faciales cargados exitosamente. Esperando instrucción de captura...")
except Exception as e:
    print("Error crítico al cargar las imágenes. Verifica que la carpeta 'fotos' exista y los nombres coincidan:", e)

# 3. Funciones del Broker MQTT
def on_connect(client, userdata, flags, rc):
    print("Conexión establecida con Mosquitto (Docker). Escuchando el canal de eventos...")
    client.subscribe(TOPIC_IMAGEN)

def on_message(client, userdata, msg):
    print("\n[NUEVO EVENTO] Frame binario recibido. Iniciando inferencia...")
    print(f"-> Tamaño del archivo recibido: {len(msg.payload)} bytes") # ¡AGREGA ESTA LÍNEA!
    
    # Si el archivo pesa menos de 100 bytes, probablemente sea un mensaje de error en texto
    if len(msg.payload) < 100:
        print(f"-> Contenido sospechoso: {msg.payload}")

    try:
        # Decodificación del buffer en memoria
        nparr = np.frombuffer(msg.payload, np.uint8)
        img = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
        if img is None:
            print("Error: Fallo de integridad en el buffer de la imagen.")
            return

        # Conversión de espacio de color BGR a RGB para la IA
        rgb_img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
        
        # Extracción de características
        face_locations = face_recognition.face_locations(rgb_img)
        face_encodings = face_recognition.face_encodings(rgb_img, face_locations)
        
        resultado = "Desconocido / Sin Persona"
        
        # Búsqueda de coincidencias
        for face_encoding in face_encodings:
            matches = face_recognition.compare_faces(rostros_conocidos, face_encoding, tolerance=0.5)
            if True in matches:
                indice = matches.index(True)
                resultado = nombres_conocidos[indice]
                break 
        
        print(f"Identidad resuelta: {resultado}. Publicando resultado...")
        
        # Transmisión de respuesta a Node-RED
        client.publish(TOPIC_RESULTADO, resultado)
        
        # Disparo de alerta si se detecta a alguien conocido
        if resultado != "Desconocido / Sin Persona":
            client.publish(TOPIC_ALERTA, f"Presencia confirmada: {resultado}")

    except Exception as e:
        print(f"Excepción controlada durante el análisis: {e}")

# 4. Arranque del Servicio
cliente = mqtt.Client()
cliente.on_connect = on_connect
cliente.on_message = on_message

cliente.connect(BROKER, PORT, 60)
cliente.loop_forever()