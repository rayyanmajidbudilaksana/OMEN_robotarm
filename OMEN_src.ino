#include <ESP32Servo.h>
#include <math.h>
#include <WiFi.h>
#include <WebServer.h>

/*WIFI Credentials*/
const char* ssid = "OMEN";
const char* password = "4DegreesOfFreedom";

// Create WebServer

WebServer server(80);

/* SERVO NAMES */
Servo J1; // Base -> thetab (XY rotation)
Servo J2; // Shoulder -> theta1
Servo J3; // Elbow -> theta2
Servo J4; // Wrist -> theta3
Servo grabservo; // Finger

/* SERVO PINS */
#define basePIN 12
#define shoulderPIN 13
#define elbowPIN 4
#define wristPIN 16
#define fingerPIN 17

/* VARIABLES */
float L1 = 95.00; // PLACEHOLDER // Link lengths in milimeters (mm) 
float L2 = 125.00;
float L3 = 91.00;

int move_time = 20; // movement time (ms) threshold for each arm movement -- goes to interpolate function 
// requires fine-tuning as of now 
double pi = 3.141592653589793; // for calculations later
// Angle Variables
double thetab; // angle var for base
double theta1;
double theta2;
double theta3;

bool isMoving = false;
unsigned long lastMoveTime;

// function declarations
void standardposition();
void grab();
void release();
void moveDirect(int j1, int j2, int j3, int j4);
void IK(int x, int y, int z, int phi);
void move();
void commandArm();
void handleDisplay();
void handleMove();

// work code

void setup() {
  Serial.begin(115200);

  // Build Soft Access Point (softAP)
  WiFi.softAP(ssid,password);

  Serial.println();
  Serial.println("OMEN Network Active!");
  Serial.print("Network name: "); Serial.println(ssid);
  Serial.print("Password: "); Serial.println(password);
  Serial.print("Connect to Wi-Fi and go to: http://");
  Serial.println(WiFi.softAPIP());  // should print 192.168.4.1

  J1.attach(basePIN);
  J2.attach(shoulderPIN);
  J3.attach(elbowPIN);
  J4.attach(wristPIN);
  grabservo.attach(fingerPIN);

  standardposition();
  delay(1000);

// Methods for WebServer
  server.on("/", handleDisplay);
  server.on("/pick", []() { //lambda function basically inplace function
    grab();
  });
  server.on("/release", []() {
    release();
  });
  server.on("/move", []() {
    handleMove();
  });
  server.on("/joint", [] () {
    int j1 = server.arg("j1").toInt();
    int j2 = server.arg("j2").toInt();
    int j3 = server.arg("j3").toInt();
    int j4 = server.arg("j4").toInt();
    moveDirect(j1, j2, j3, j4);
  });
  server.begin();
  Serial.println("Server ready");
}

void loop() {
  server.handleClient();

  if (isMoving && millis() - lastMoveTime >= move_time) {
    isMoving = false;
  }
}

void handleMove() {
  if (!isMoving) {
    commandArm();
    lastMoveTime = millis();
    isMoving = true;
  }
}

// Web Display
void handleDisplay() {
  String html = R"rawliteral(
  <html>
  <head>
    <title> OMEN Control Panel </title>
    <style>
      body {
        background-color: #2D262A;
        color: white;
        font-family: Arial;
        text-align: center;
      }
      
      /* Slider X block */
      .slider {
      -webkit-appearance: none;
      width: 50%;
      height: 20px;
      background: white;
      border-radius: 5px;
      outline: black;
      opacity: 1;
      transition: opacity 0.2s;
      }
      
      .slider:hover {
      opacity: 1;
    }

    /* Slider Thumb */
    .slider::-webkit-slider-thumb {
      -webkit-appearance: none;
      appearance: none;
      width: 25px;
      height: 30px;
      background: #FA390E;
      border-radius: 10%;
      cursor: pointer;
    }

    .slider::-moz-range-thumb {
      width: 20px;
      height: 20px;
      background: #4CAF50;
      border-radius: 50%;
      cursor: pointer;
    }
    .preset { /* preset button */
      background-color: #FA390E;
      color: white;
      padding: 10px 20px;
      border: none;
      border-radius: 5px;
      cursor: pointer;
    
    }
    .button { /* preset button */
      background-color: #85C148;
      color: white;
      padding: 10px 20px;
      border: none;
      border-radius: 5px;
      cursor: pointer;
    
    }
    </style>
  </head>
  <body>
  <h1>OMEN</h1>
  <label>Control Mode: </label>
  <button id="modeswitch" class="preset"> IK </button>
  
  
  
  <div id="IKControl">
  <h2>X: <span id="valX"> 180 </span> </h2>
  <input type="range" id="myX" min="-335" max="335" value="180" class = "slider">

  <h2>Y: <span id="valY"> 200 </span></h2>
  <input type="range" id="myY" min="0" max="335" value="200" class = "slider">
  
  <h2>Z: <span id="valZ"> 200 </span></h2>
  <input type="range" id="myZ" min="0" max="335" value="200" class = "slider">

  <h2>Orientation: <span id="valOri"> 0 </span></h2>
  <input type="range" id="myOri" min="-90" max="90" value="0" class = "slider">
  
  </div>
  
  <div id="FKControl" style="display:none;"> 
  <h2>J1 <span id="valJ1"> 90 </span>deg</h2>
  <input type="range" id="myJ1" min="0" max="180" value="90" class = "slider">
  
  <h2>J2 <span id="valJ2"> 90 </span>deg</h2>
  <input type="range" id="myJ2" min="0" max="180" value="90" class = "slider">
  
  <h2>J3 <span id="valJ3"> 90 </span>deg</h2>
  <input type="range" id="myJ3" min="0" max="180" value="90" class = "slider">
  
  <h2>J4 <span id="valJ4"> 90 </span>deg</h2>
  <input type="range" id="myJ4" min="0" max="180" value="90" class = "slider">
  </div>
  <br><br>
  <button class="button" onclick="fetch('/pick')"><h2>Pick</h2></button>
  <button class="button" onclick="fetch('/release')"><h2>Release</h2></button>
  
  <script>
    let mode = "IK"; // default mode

  const modeSwitch = document.getElementById("modeswitch");
  const IKControl = document.getElementById("IKControl");
  const FKControl = document.getElementById("FKControl");

  // --- Add event listeners for IK sliders ---
  document.querySelectorAll("#IKControl input[type=range]").forEach(slider => {
    slider.addEventListener("input", () => {
      let x = document.getElementById("myX").value;
      let y = document.getElementById("myY").value;
      let z = document.getElementById("myZ").value;
      let ori = document.getElementById("myOri").value;

      // update displayed values
      document.getElementById("valX").innerText = x;
      document.getElementById("valY").innerText = y;
      document.getElementById("valZ").innerText = z;
      document.getElementById("valOri").innerText = ori;

      // send IK command
      fetch(`/move?x=${x}&y=${y}&z=${z}&ori=${ori}`);
    });
  });

  // --- Add event listeners for FK sliders ---
  document.querySelectorAll("#FKControl input[type=range]").forEach(slider => {
    slider.addEventListener("input", () => {
      let j1 = document.getElementById("myJ1").value;
      let j2 = document.getElementById("myJ2").value;
      let j3 = document.getElementById("myJ3").value;
      let j4 = document.getElementById("myJ4").value;

      // update displayed values
      document.getElementById("valJ1").innerText = j1;
      document.getElementById("valJ2").innerText = j2;
      document.getElementById("valJ3").innerText = j3;
      document.getElementById("valJ4").innerText = j4;

      // send FK command
      fetch(`/joint?j1=${j1}&j2=${j2}&j3=${j3}&j4=${j4}`);
    });
  });

  // --- Mode switch button ---
  modeSwitch.addEventListener("click", () => {
    if (mode === "IK") {
      mode = "FK";
      IKControl.style.display = "none";
      FKControl.style.display = "block";
      modeSwitch.innerText = "FK";
    } else {
      mode = "IK";
      IKControl.style.display = "block";
      FKControl.style.display = "none";
      modeSwitch.innerText = "IK";
    }
  });
  </script>
  </body>
  
  </html>
  )rawliteral";
  server.send(200, "text/html", html);
}

void commandArm() {
    int x = server.arg("x").toInt();
    int y = server.arg("y").toInt();
    int z = server.arg("z").toInt();
    int ori = server.arg("ori").toInt();

    IK(x,y,z,ori);
    move();
  }
  // server.send(200, "text/plain", "Moved");


// Inverse Kinematics Function
void IK(int x, int y, int z, int phi) { // still 2D
// REMEMBER THAT IT IS CALCULATED IN RADIANS
  double phi_rad = phi * PI / 180.0;
  double x_wrist = x - L3*cos(phi_rad); // cannot be int -> will lose precision
  double y_wrist = y - L3*sin(phi_rad);

// THETA2
  double cval = (pow(x_wrist, 2) + pow(y_wrist, 2) - pow(L1, 2) - pow(L2, 2)) / (2*L1*L2);
  // Handle edge cases for arccos domain where -1 =< f(x) =< 1
  if (cval > 1) {
    cval = 1;
  } else if (cval < -1) {
    cval = -1;
  }
  double theta2_rad = -acos(cval); //elbow up +acos || elbow down -acos
// -----------------------

// THETA1
  double A =  L1 + L2*cos(theta2_rad); 
  double B = L2 * sin(theta2_rad);
  double theta1_rad = atan2(y_wrist, x_wrist) - atan2(L2*sin(theta2_rad), L1 + L2 * cos(theta2_rad));
  //double theta1_rad = atan2(-B * x_wrist + A * y_wrist , A * x_wrist + B * y_wrist);  alternative equation
//-----------------------

// THETA3
  double theta3_rad = phi_rad - theta2_rad - theta1_rad;
//-----------------------

  //double thetab_rad = atan( z / x );
  // CONVERT TO DEGREES
  //thetab = thetab_rad * 180.0 / pi;
  theta1 = theta1_rad * 180.0 / pi;
  theta2 = theta2_rad * 180.0 / pi;
  theta3 = theta3_rad * 180.0 / pi;

  // take account of offset on J3 and J4 (theta2 & 3)
  //thetab += 90;
  theta1 += 0;
  theta2 += 90;
  theta3 += 90;
  return; 
}




// INTERPOLATE FUNCTION
void move() {
  //int i1 = J1.read(); 
  int i2 = J2.read();
  int i3 = J3.read();
  int i4 = J4.read();

  // angle changes for each servo
  //int d1 = thetab - i1;
  int d2 = theta1 - i2;
  int d3 = theta2 - i3;
  int d4 = theta3 - i4;

  unsigned long init_time = millis();
  
  unsigned long curr_time;

  while ((curr_time = millis())- init_time < move_time ) {   // See below for loop function explanation
    float t = float(curr_time - init_time) / move_time; // movetime threshold see variable section line 23
    //J1.write(i1 + d1 * t); 
    J2.write(i2 + d2 * t);
    J3.write(i3 + d3 * t);
    J4.write(i4 + d4 * t);
  }

  delay(15); // set delay ms

  //rewrite to make sure
  //J1.write(thetab); //-- J1 ignored currently due to math not done
  J2.write(theta1);
  J3.write(theta2);
  J4.write(theta3);
}
/* move() interpolation function block EXPLAINED
variables init_time saves the time when the function was called from millis()
curr_time variable is updated constantly through each loop iteration from millis();
move_time is a threshold for how long one whole movement command takes
variable t is just delta time (dt) / move_time

Jx.write(ix + dx * t) writes new angles to the servo, each servo will move to its target angle at the same exact time as the other servo.
After that, Jx.write(thetax) is rewritten just to make sure it is at the right position.
*/

void moveDirect(int j1, int j2, int j3, int j4) {
  //J1.write(j1);
  J2.write(j2);
  J3.write(j3);
  J4.write(j4);
}

void standardposition() {
  J1.write(90);
  J2.write(90);
  J3.write(90);
  J4.write(90);
  grabservo.write(90);
}

void grab() {
  grabservo.write(180);
  grabservo.detach();
}

void release() {
  grabservo.attach(fingerPIN);
  grabservo.write(90);
}



/* NEXT notes
Interpolate the servos so they all move simultaneously. 
0first moving one servo a little, then moving the next one, back and forth until all servos arrive at their destination point.
*/


//Function Dump/Storage for Unused functions



