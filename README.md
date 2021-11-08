# OctoprintProgressDisplay
Device to display the progress of the actual job in Octoprint. 2 7-Segments are used as an display. To control the 7-Segments, each is connected to an Shiftregister SN74HC595N. An AZ-Delivery D1-Mini is used to fetch data from Octopint via the API and control hte 7-Segments.
To get Wifi-Conection a WifiManager form tzapu is used.

## How to install
1.  Copy secret_template.h and rename it to secrets.h
2.  Replace APIKEY, HOST, PORT with your credentials.
3.  Flash D1-Mini

## How it works
1.  Connect D1-Mini to 5v e.g. via USB
2.  If the dots are flashing the D1 trys to connect to Wifi. Check if new configuration is needen.
<br>
<img src="https://raw.githubusercontent.com/SiggiSigmann/OctoprintProgressDisplay/main/img/Display_ReadyToConnect.gif" alt="ReadyToConnect" title="ReadyToConnect" width="25%" style="transform:rotate(-90deg);"/>


3. Connect to wifi with the name **OctoPrintProgressDisplay**
<br>
<img src="https://raw.githubusercontent.com/SiggiSigmann/OctoprintProgressDisplay/main/img/wifiManager_Step1.jpg" alt="ConnectToWifi" title="ConnectToWifi" width="25%"/>

4. Go to Sign in Page and selekt **Configure Wifi**
<br>
<img src="https://raw.githubusercontent.com/SiggiSigmann/OctoprintProgressDisplay/main/img/wifiManager_Step2.jpg" alt="SignIn" title="SignIn" width="25%"/>

5. Connect to Wifi
<br>
<img src="https://raw.githubusercontent.com/SiggiSigmann/OctoprintProgressDisplay/main/img/wifiManager_Step3.jpg" alt="ConnectToWifi" title="ConnectToWifi" width="25%"/>

6. Done. The Connection will be stored to reuse next time
<br>
<img src="https://raw.githubusercontent.com/SiggiSigmann/OctoprintProgressDisplay/main/img/wifiManager_Step4.jpg" alt="Done" title="Done" width="25%"/>

7.  If the devoice can't connect to Octoprint it will display error 
<br>
<img src="https://raw.githubusercontent.com/SiggiSigmann/OctoprintProgressDisplay/main/img/Display_error.jpg" alt="Error" title="Error" width="25%" style="transform:rotate(-90deg);"/>

8. While heating it will start to draw ciruals. The circul will be complete when heating is done.<br>
<img src="https://raw.githubusercontent.com/SiggiSigmann/OctoprintProgressDisplay/main/img/Display_idel.jpg" alt="Heating" title="Heating" width="25%" style="transform:rotate(-90deg);"/>

9. The display shows the progess in percent while printing

10. If Octoprint is in idel mode this will be displayed.<br>
<img src="https://raw.githubusercontent.com/SiggiSigmann/OctoprintProgressDisplay/main/img/Display_idel.jpg" alt="Idel" title="Idel" width="25%" style="transform:rotate(-90deg);"/>
## Circuit
<img src="https://raw.githubusercontent.com/SiggiSigmann/OctoprintProgressDisplay/main/img/circuit.jpg)" alt="Circuit" title="Circuit" />
Made with https://easyeda.com/