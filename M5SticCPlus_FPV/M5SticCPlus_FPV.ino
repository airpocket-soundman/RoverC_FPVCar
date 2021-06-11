#include <M5StickCPlus.h>
#include <Ps3Controller.h>

//int player = 0;
//int battery = 0;
#define CAMMAX 440
#define CAMCENTER 300
#define CAMMIN 200
#define CAMUNDER 160
#define AXISMAX 128

int vx = 0;     //横移動速度        left -100～100 right　
int vy = 0;     //前後移動速度      back -100～100 front
int vr = 0;     //回転速度          left -100～100 right
int CAMpitch = CAMCENTER; //カメラ上下位置   200～440
const int tiltpin = 32;

HardwareSerial VSerial(1);

uint8_t I2CWrite1Byte(uint8_t Addr, uint8_t Data)
{
    Wire.beginTransmission(0x38);
    Wire.write(Addr);
    Wire.write(Data);
    return Wire.endTransmission();
}

uint8_t I2CWritebuff(uint8_t Addr, uint8_t *Data, uint16_t Length)
{
    Wire.beginTransmission(0x38);
    Wire.write(Addr);
    for (int i = 0; i < Length; i++)
    {
        Wire.write(Data[i]);
    }
    return Wire.endTransmission();
}

uint8_t Setspeed()
{
    int16_t Vtx = vx;
    int16_t Vty = vy;
    int16_t Wt = vr / 3;
    int16_t speed_buff[4] = {0};
    int8_t speed_sendbuff[4] = {0};
    if (abs(Vtx) <= 10)
    {
        Vtx = 0;
    }
    if (abs(Vty) <= 10)
    {
        Vty = 0;
    }
    if (abs(Wt) <= 5)
    {
        Wt = 0;
    }
    Serial.print(Vtx);
    Serial.print(",");
    Serial.print(Vty);
    Serial.print(",");
    Serial.print(Wt);
    Serial.print(",");
    Serial.print(CAMpitch);
    Serial.print(" / ");

    Wt = (Wt > 100) ? 100 : Wt;
    Wt = (Wt < -100) ? -100 : Wt;
    Vtx = (Vtx > 100) ? 100 : Vtx;
    Vtx = (Vtx < -100) ? -100 : Vtx;
    Vty = (Vty > 100) ? 100 : Vty;
    Vty = (Vty < -100) ? -100 : Vty;

    Serial.print(Vtx);
    Serial.print(",");
    Serial.print(Vty);
    Serial.print(",");
    Serial.print(Wt);
    Serial.print(",");
    Serial.println(CAMpitch);
    M5.Lcd.fillScreen(GREEN);
    M5.Lcd.drawString("Vtx=",0,10);
    M5.Lcd.drawNumber(Vtx,50,10);
    M5.Lcd.drawString("Vty=",0,20);
    M5.Lcd.drawNumber(Vty,50,20);
    M5.Lcd.drawString("Wt=",0,30);
    M5.Lcd.drawNumber(Wt,50,30);
    M5.Lcd.drawString("pitch=",0,40);
    M5.Lcd.drawNumber(CAMpitch,50,40);

    Vtx = (Wt != 0) ? Vtx * (100 - abs(Wt)) / 100 : Vtx;
    Vty = (Wt != 0) ? Vty * (100 - abs(Wt)) / 100 : Vty;

    speed_buff[0] = (Vty - Vtx - Wt) * 1.0;    //left front モーターの出力を補正するために*1.0
    speed_buff[1] = (Vty + Vtx + Wt) * 1.15;   //right front  同じく*1.5
    speed_buff[3] = (Vty - Vtx + Wt) * 1.15;   //right back　同じく*1.5
    speed_buff[2] = (Vty + Vtx - Wt) * 1.0;    //left back　同じく*1.0 ※この補正値はroverCの機体毎に要調整

    ledcWrite(0,CAMpitch);

    for (int i = 0; i < 4; i++)
    {
        speed_buff[i] = (speed_buff[i] > 100) ? 100 : speed_buff[i];
        speed_buff[i] = (speed_buff[i] < -100) ? -100 : speed_buff[i];
        speed_sendbuff[i] = speed_buff[i];
    }
    return I2CWritebuff(0x00, (uint8_t *)speed_sendbuff, 4);
}

void notify()
{

    //---------------- Analog stick value events ---------------
    if( abs(Ps3.event.analog_changed.stick.lx) + abs(Ps3.event.analog_changed.stick.ly) > 2 ){
        vy = -Ps3.data.analog.stick.ly;
        Setspeed();
    }

    if( abs(Ps3.event.analog_changed.stick.rx) + abs(Ps3.event.analog_changed.stick.ry) > 2 ){
        vr = -Ps3.data.analog.stick.rx;
        Setspeed();
    }

    //--------------- Analog D-pad button events ----------------
    if( abs(Ps3.event.analog_changed.button.up) ){
        CAMpitch += Ps3.data.analog.button.up / 20;
        if(CAMpitch > CAMMAX){
            CAMpitch = CAMMAX;
        }
        Setspeed();
    }

    if( abs(Ps3.event.analog_changed.button.down) ){
        CAMpitch -= Ps3.data.analog.button.down / 20;
        if(CAMpitch < CAMMIN){
           CAMpitch = CAMMIN;
        }
        Setspeed();
    }

    if( abs(Ps3.event.analog_changed.button.right) ){
        CAMpitch = CAMCENTER;
        Setspeed();
    }

    if( abs(Ps3.event.analog_changed.button.left) ){
        CAMpitch = CAMCENTER;
        Setspeed();
    }

    //---------- Analog shoulder/trigger button events ----------
    if( abs(Ps3.event.analog_changed.button.l1)){
        vx = -Ps3.data.analog.button.l1;
        Setspeed();
    }

    if( abs(Ps3.event.analog_changed.button.r1) ){
        vx = Ps3.data.analog.button.r1;
        Setspeed();
    }
}

void onConnect(){
    Serial.println("Connected.");
}

void setup()
{

    ledcSetup(0,50,12);
    ledcAttachPin(tiltpin,0);

    M5.begin();
    M5.Lcd.setRotation(3);
    M5.Lcd.fillScreen(GREEN);

    Wire.begin(0, 26);
    Ps3.attach(notify);
    Ps3.attachOnConnect(onConnect);
    Ps3.begin("94:B9:7E:8C:83:7A"); //このプログラムを書き込むM5StickC plseのmacアドレス
//    Setspeed(vx,vy,vr);

}


void loop()
{
    delay(10000);
}
