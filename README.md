# SmartHome UTalca - Dashboard Node-RED

Este repositorio contiene el frontend del proyecto SmartHome UTalca, desarrollado por el Grupo 

El proyecto implementa una interfaz de monitoreo y control para un hogar inteligente utilizando Node-RED y el protocolo MQTT. El dashboard está diseñado con una estructura modular para la visualización de datos en tiempo real.

## Características Principales

* Monitoreo Ambiental: Visualización dinámica de Temperatura, Humedad, concentración de Gas y Nivel de Ruido (dB).
* Módulo de Seguridad: Interfaz configurada para la recepción de imágenes desde una ESP32-CAM y registro de alertas de detección de personas y movimiento.
* Control Manual: Actuadores integrados para la captura remota de fotografías y control de iluminación.
* Comunicación IoT: Nodos MQTT configurados mediante el broker público HiveMQ para la integración con los microcontroladores ESP32.
* Entorno de Simulación: Incluye un generador de telemetría (Mock Data) que inyecta valores ambientales en rangos realistas cada 3 segundos, permitiendo realizar pruebas de la interfaz sin el hardware físico.

## Requisitos Previos

Para ejecutar este dashboard en un entorno local, se requiere:
1. Node.js y Node-RED instalados.
2. El paquete de UI de Node-RED. Se puede instalar desde "Manage palette" buscando e instalando `node-red-dashboard`.

## Instrucciones de Instalación

Para cargar el flujo de trabajo en Node-RED, sigue estos pasos:

1. Descarga el archivo `flows.json` de este repositorio.
2. Abre la interfaz de desarrollo de Node-RED en tu navegador (usualmente `http://localhost:1880`).
3. Dirígete al menú superior derecho y selecciona la opción "Importar".
4. Selecciona el archivo `flows.json` descargado o pega el código en texto plano en la ventana de importación.
5. Selecciona "Importar".
6. Haz clic en el botón "Instanciar" (Deploy) en la esquina superior derecha para guardar los cambios y ejecutar el flujo.

## Acceso a la Interfaz

Una vez instanciado el servidor, puedes acceder a la interfaz gráfica del dashboard navegando a la siguiente ruta:

http://localhost:1880/ui

Para verificar la simulación de datos, comprueba que los nodos "Inject" del flujo estén activos; la interfaz comenzará a actualizar los valores de los medidores automáticamente a través de la suscripción al broker MQTT.

---
Desarrollado para el proyecto de título/módulo de integración - Ingeniería Civil en Computación, Universidad de Talca.