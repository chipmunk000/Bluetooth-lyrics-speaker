#include <lvgl.h>
#include <WiFi.h>
#include <TFT_eSPI.h>
#include "demos\lv_demos.h"
#include "ui.h"
#include "ui_comp_alarm_comp.h"
#include "ui_comp_clock_dot.h"
#include "ui_comp_hook.h"
#include "ui_comp_scrolldots.h"
#include "ui_comp_small_label.h"
#include "ui_comp.h"
#include "ui_events.h"
#include "ui_helpers.h"
#include "config.h"

#define NTP1  "ntp1.aliyun.com"
#define NTP2  "ntp2.aliyun.com"
#define NTP3  "ntp3.aliyun.com"

#define BTN0  35
#define BTN1  19
#define BTN2  37

int8_t btn_act;

static lv_disp_draw_buf_t draw_buf;    //定义显示器变量
static lv_color_t buf[TFT_WIDTH * 10]; //定义刷新缓存

TFT_eSPI tft = TFT_eSPI();

typedef void (*Demo)(void);
//I2S管脚配置
int bck = 14; 
int ws = 13;
int dout = 12;

String last_title = "";
String last_artist = "";
uint8_t flag_first = 0;

extern  char music_lyrics[100];
extern  char music_artist[50];

std::chrono::steady_clock::time_point _audioUpdate;
//修改蓝牙名称
btAudio audio = btAudio("test");


void audio_Lyrics_update(uint16_t duration)
{
    int gbk_len = 0;
    auto now = std::chrono::steady_clock::now();
    int64_t dt = std::chrono::duration_cast<std::chrono::milliseconds>(now - _audioUpdate).count();
    if(dt >= duration)
    {
        _audioUpdate = now;
        audio.updateMeta();
        if(audio.artist.compareTo(last_artist) != 0){
             Serial.print("Artist: ");
             Serial.println(audio.artist);
             Serial.print("Album: ");
             Serial.println(audio.album);
             Serial.print("Genre: ");
             Serial.println(audio.genre);    
             strncpy(music_artist, audio.artist.c_str(), 49); 
             last_artist = audio.artist;
        }
        if(audio.title.compareTo(last_title)!=0){
            strncpy(music_lyrics, audio.title.c_str(), 99);
         
            Serial.print("Title: ");
            Serial.println(music_lyrics);
            last_title = audio.title;           
        }
    }
}

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
 
    tft.startWrite();                                        //使能写功能
    tft.setAddrWindow(area->x1, area->y1, w, h);             //设置填充区域
    tft.pushColors((uint16_t *)&color_p->full, w * h, true); //写入颜色缓存和缓存大小
    tft.endWrite();                                          //关闭写功能
 
    lv_disp_flush_ready(disp); //调用区域填充颜色函数
}

//按键信息
typedef struct
{
unsigned char id;
bool pressed;
}KEY_MSG;

//按键变量
typedef struct
{
  bool val;
  bool last_val;  
}KEY;

KEY_MSG key_msg={0};
KEY key[3]={false};

bool get_key_val(uint8_t ch)
{
  switch (ch)
  {
    case 0:
    return digitalRead(BTN0);
    break;
    case 1:
    return digitalRead(BTN1);
    break;
    case 2:
    return digitalRead(BTN2);
    break;
    default:
    break;
  }
}

void key_init()
{
  for(uint8_t i=0;i<(sizeof(key)/sizeof(KEY));++i)
  {
    key[i].val=key[i].last_val=get_key_val(i);
  }
}

void key_scan()
{
    for(uint8_t i=0;i<(sizeof(key)/sizeof(KEY));++i)
  {
    key[i].val=get_key_val(i);//获取键值
    if(key[i].last_val!=key[i].val)//发生改变
    {
      key[i].last_val=key[i].val;//更新状态
      if(key[i].val==LOW)
      {
      key_msg.id=i;
      key_msg.pressed=true;
      }
    }
  }
  
}

//按键操作后返回的状态值

static void button_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    
    static uint8_t last_btn = 0;

    /*Get the pressed button's ID*/
    btn_act = key_msg.id;
    if(btn_act >= 0) {
        data->state = LV_INDEV_STATE_PR;
        last_btn = btn_act;
        key_msg.id = 0;
    }
    else {
        data->state = LV_INDEV_STATE_REL;
    }

    /*Save the last pressed button's ID*/
    data->btn_id = last_btn;
}

static lv_indev_t *indev_button; //定义一个按键
// 输入设备初识化函数
void lv_port_indev_init(void)
{
	// 注册输入设备
	static lv_indev_drv_t indev_drv;
	lv_indev_drv_init( &indev_drv ); 
	indev_drv.type = LV_INDEV_TYPE_BUTTON; //定义为Button类型
	indev_drv.read_cb = button_read; //绑定按键值返回函数
	indev_button = lv_indev_drv_register( &indev_drv ); //注册按键
  static const lv_point_t btn_points[1] = {
    {204,18},//这个就是按键对应的一个屏幕上按键组件的中心坐标点
  };
  lv_indev_set_button_points(indev_button,btn_points); //将实体按键和坐标点绑定
}

/*********WIFI***********/
 
const char *ssid     = "";
const char *password = "";

struct tm timeinfo;
int hour,minute,second,year,mouth,day;

void time_show()
{
    hour = timeinfo.tm_hour;
    minute = timeinfo.tm_min;
}

void setClock() {
    if (!getLocalTime(&timeinfo))
    {//如果获取失败，就开启联网模式，获取时间
        Serial.println("Failed to obtain time");
     //    WiFi.disconnect(false);
        WiFi.mode(WIFI_STA);//开启网络  
        WiFi.begin(ssid, password);
         while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
        
    }
     configTime(8 * 3600, 0, NTP1, NTP2,NTP3);
        return;
    }
    WiFi.disconnect(true);//断开网络连接，关闭网络
    time_show();   
}

void TIME_INIT()
{
   //设置ESP32工作模式为无线终端模式
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("WiFi connected!");
 configTime(8 * 3600, 0, NTP1, NTP2,NTP3);
      setClock();
    // 从网络时间服务器上获取并设置时间
    // 获取成功后芯片会使用RTC时钟保持时间的更新  
    WiFi.disconnect(true);//断开wifi网络
    WiFi.mode(WIFI_OFF);//关闭网络
    Serial.println("WiFi disconnected!");

}



void setup()
{
    tft.init();         //初始化
    tft.setRotation(4); //屏幕旋转方向（横向）
    Serial.begin(9600);
    audio.begin();
    audio.I2S(bck, dout, ws);
    audio.reconnect();  
    TIME_INIT();
    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, TFT_WIDTH * 10);

    pinMode(BTN0, INPUT_PULLDOWN);
    pinMode(BTN1, INPUT_PULLDOWN);
    pinMode(BTN2, INPUT_PULLDOWN);
    key_init();
    
    /*Initialize the display*/
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    /*Change the following line to your display resolution*/
    disp_drv.hor_res = TFT_WIDTH;
    disp_drv.ver_res = TFT_HEIGHT;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
    lv_port_indev_init();
    
     ui_init();
     
}
 
void loop()
{       
   
    lv_timer_handler(); 
    delay(5);
    key_scan();
    setClock();
    audio_Lyrics_update(100);

}
