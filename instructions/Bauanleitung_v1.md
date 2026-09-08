# openLUXbox
## Bauanleitung V1
Diese Anleitung beschreibt den Bau der openLUXbox v1. Alle benötigten Teile können selbst bestellt werden (siehe Teileliste). Case + Cover müssen 3d-gedruckt werden. Dieser vereinfache Aufbau ist inspiriert von 1ST1 aus dem [Homecon-Forum](https://forum.homecon.org/). 
### Case + Cover
Beide Teile sind mit PLA gedruckt. 
Das Case wird in einem Stück gedruckt. Dabei sind Stützen zu aktivieren. 
Das Cover besteht ebenfalls aus PLA. Die ersten Schichten sind einfach weißes PLA, das trennende Raster schwarzes PLA. Ohne Multifarbdrucker ist beim Slicen mit dem Beginn des Rasters ein Farbwechsel einzustellen.

![Gehäuse](case.jpg)

### Verkabelung
Der ESP8266 wird später in diese Halterung geschoben.

![ESP](esp.jpg)

Die Stromversorgung erfolgt über eine USB-C-Buchse, an die ein Stecker angelötet wird. Dieser versorgt den ESP mit +5V/GND und zwei Datenleitungen, um Spiele zu flashen.

![Power Connector](power_connector1.jpg)
![Power Connector](power_connector2.jpg)

Die Buttons haben einen Distanzring und eine Gewindemutter. Zwei Kabel (ca. 20cm) werden angelötet (oder sind es bereits).

![Button](button1.jpg)

Die Mutter wird von unten in den Halterungen der Buttons gesetzt.

![Button](button2.jpg)

Der Buttton wird dann von oben mit Distanzring und Kabeln eingeführt und in die Mutter eingedreht. Sitzt der Button fest, werden die beiden Kabel durch die Öffnung nach innen geführt.

![Button](button3.jpg)
![Button](button4.jpg)
![Button](button5.jpg)

Die roten Kabel der Buttons entweder direkt an den ESP gelötet werden. Alternativ (wie im Bild) lötet man kurze Verbindungskabel in der jeweiligen Buttonfarbe an den ESP. Diese werden dann mit den roten Kabeln des jeweiligen Buttons verbunden.

![Button_esp](button_esp1.jpg)
![Button_esp](button_esp2.jpg)

Zwischen ESP und die Datenverbindung des LED (grünes Kabel) wird der Widerstand gelötet.

![resistor](resistor.jpg)

Hier die vollständige Beschaltung des ESP.

PIN 4 ist dabei der grüne Button. PIN 2 die Datenverbindung zum LED.

![complete1](complete1.jpg)
![complete2](complete2.jpg)

Der Kondensator und die Spannungsversorgung des LED-Panels werden direkt an die USB-C-Buchse (von unten) gelötet.

![capacitor](capacitor.jpg)
![led_power](led_power.jpg)

Die vier schwarzen Kabel der Buttons (GRD) werden miteinander und einem weiteren Kabel verlötet. Das weitere Kabel kommt ebenfalls an GRD der USB-C-Buchse. Alle fünf Kabel am besten mit Schrumpfschlauch schützen.

![button_grd1](button_grd1.jpg)
![button_grd2](button_grd2.jpg)

Der komplette Aufbau sieht dann so aus.

![final](final.jpg)





