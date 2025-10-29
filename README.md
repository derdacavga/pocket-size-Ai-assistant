🧠 Dsn AI Voice Assistant v2

Much More Responsive, Brilliant, and Portable.

Welcome to the second version of my ESP32-based AI voice assistant project — Dsn AI Assistant!
This version is faster, smarter, and more optimized for portability.

📺 Before you continue, don’t forget to Like
 👍 and Subscribe
!
That’s important to keep the project growing.

🎙️ Introduction

Hello everyone!
In this project, I’m introducing a new ESP32-based AI Assistant — the second version of my previous design.

🧩 If you haven’t watched the first version, check it out here
.
It includes the full server setup, installation, and configuration details.

The AI model is hosted on Hugging Face, because the ESP32’s flash, RAM, and clock speed are not sufficient to run large models locally.
Instead, the ESP32 acts as a client, handling voice input and audio response playback.

This version includes:

⚙️ New hardware setup

🧠 Improved AI model

💬 Real-time internet-connected LLM responses

💖 Patreon community and exclusive updates

🧩 Components Used
Component	Description
ESP32-C3	Custom dev board (you can use any ESP32-C3 board)
INMP441	I2S MEMS Microphone
MAX98357A	I2S Audio Amplifier
SSD1306	OLED Display (I²C)

⚠️ Note: ESP32-C3 has fewer GPIO pins and less SRAM compared to other ESP32 models.
I optimized memory usage, GPIO mapping, and buffer values after multiple iterations.

🧠 Pin & Setup Notes

The I2S microphone and I2S amplifier share some pins (except DIN and DOUT).

The MAX98357A SD pin is connected to 3.3V (always ON).

The OLED uses only 2 pins — perfect for low-pin-count boards.

You can find the complete wiring diagram in the image below 👇

(Add your wiring image here)
![Wiring Diagram](images/wiring_diagram.png)

💻 Software Overview

In the previous version, the ESP32:

Downloaded an MP3 audio file from the server to SPIFFS

Then played it locally

Now, in Version 2, it:

🎧 Streams WAV audio directly from the server

⚡ Plays the response instantly — much faster!

🌍 Connects to the internet to provide real-time answers using LLM

🗣️ Voice commands start with a button press (hands-free mode planned for the next version)

🧠 Improvements Summary
Feature	Previous	New Version
Audio Format	MP3 (download & play)	WAV (live stream)
Response Time	Slow	Instant
Model Source	Local files	Hugging Face
Connectivity	Offline	Web-connected (real-time)
Display	TFT	OLED (2-pin minimal)
Control	Button	Button / Planned Hands-free
📁 Files & Resources

🧩 Source Code: GitHub Repository

🧰 3D Models: Cults3D Page

💖 Support Me on Patreon: Patreon Page

Get early access, detailed tutorials, and exclusive Patreon-only projects!

🧱 Making It Portable

I designed a custom shell in Fusion 360 to make the assistant truly portable.
You can download the model file from the link in the description.

After assembling all components, I noticed a little speaker noise —
The I2S setup pushes the ESP32-C3 pretty hard, but I’ll fix that in the next revision.

🧩 Feedback & Next Steps

💬 Write in the comments — what do you want to see in the next version?
I’m considering:

Hands-free voice activation

Battery optimization

Advanced OLED animations

Stay tuned for the next project — see you soon! 🚀

🧑‍💻 Author

Dsn Engineering
ESP32 • AI • Embedded Systems • 3D Design
