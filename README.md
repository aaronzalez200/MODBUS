MODBUS prototype using an Arduino MEGA (using WINAVR, not Arduino IDE), and Wiznet's WIZ850io. 
Code includes arrays that act as the MODBUS registers/coils/inputs.

Navigating to ioLibrary_Driver > Application > modus
This location will contain two files that I created to add MODBUS functionality to the WIZNET library.

This repository was created to document my learning with the MODBUS protocol over TCP. There are a few
bugs present.
![MODBUS Doc](https://github.com/user-attachments/assets/8f047334-e08b-4b00-a100-82f913a11f29)
Below is the setup with the mentioned hardware, breadboard, and thermistor
![modbus hardware](https://github.com/user-attachments/assets/3393ac07-8a13-4067-995e-3d5199eedd3d)

There are bugs in the code where some combinations of address and quantity result in the data not being received correctly. The main goal of this project was to learn the Modbus protocol, so the implementation may be incomplete or non-compliant with all edge cases. Below are examples using MODBUS POLL to make sure the function code, address, and quantity combination we request is being processed by the Arduino MEGA correctly and sent back to MODBUS POLL.

The image below was produced using function code 1, Address 1, Quantity 16
![image](https://github.com/user-attachments/assets/99f3d13c-db99-4b87-bf78-232de4212894)

Then the parameters are updated with Address 5, Quantity 8
![image](https://github.com/user-attachments/assets/da6c9ee0-743b-4f97-b765-48cb979c44c9)

Then updated to function code 2, Address 4, Quantity 8
![image](https://github.com/user-attachments/assets/b25fc05e-f43e-44ae-b701-fb8c602a6ef6)

Then updated to function code 3, Address 0, Quantity 4
![image](https://github.com/user-attachments/assets/a9859004-11f9-4596-91b6-eca549d884c2)

Below is a video of MODBUS POLL making requests to the Holding Registers, which store the temperature read by a thermistor @ Address 1:
https://github.com/user-attachments/assets/8064c62f-daa5-4c4f-a70b-feb19528f051

Lastly, below is a video of MODBUS POLL making single coil writes which turns on/off LEDs on the breadboard if writing to address 1 (LED #1) or address 3 (LED #2):
The video is sped up to be attached here. 
https://github.com/user-attachments/assets/1b770aec-e7a6-43af-a7b2-b6147f2bbe93

