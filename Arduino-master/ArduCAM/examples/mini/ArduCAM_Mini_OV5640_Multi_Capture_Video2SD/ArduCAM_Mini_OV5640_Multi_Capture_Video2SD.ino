// ArduCAM Mini demo (C)2016 Lee
// web: http://www.ArduCAM.com
// This program is a demo of how to use the enhanced functions
// of the library with ArduCAM MINI UNO 5MP camera, and can run on any Arduino platform.
// This demo was made for ArduCAM MINI UNO OV5640 5MP Camera.
// It can  continue shooting  and store it into the SD card  in AVI format
// The demo sketch will do the following tasks
// 1. Set the camera to JEPG output mode.
// 2. Capture a JPEG photo and buffer the image to FIFO
// 3.Write AVI Header
// 4.Write the video data to the SD card
// 5.More updates AVI file header
// 6.close the file
//The file header introduction
//00-03 :RIFF
//04-07 :The size of the data
//08-0B :File identifier
//0C-0F :The first list of identification number
//10-13 :The size of the first list
//14-17 :The hdr1 of identification
//18-1B :Hdr1 contains avih piece of identification 
//1C-1F :The size of the avih
//20-23 :Maintain time per frame picture

// This program requires the ArduCAM V3.4.1 (or later) library and ArduCAM ESP8266 5MP camera
// and use Arduino IDE 1.5.8 compiler or above



#include <Wire.h>
#include <ArduCAM.h>
#include <SPI.h>
#include <SD.h>
#include "memorysaver.h"

#define BMPIMAGEOFFSET  66
#define   FIFO_SIZE     0x07FFFFF
#define   FRAMES_NUM    0x07
#define   FRAME_RATE     0x05
// set pin 10 as the slave select for the digital pot:
const int CS = 7;
#define SD_CS 9
bool is_header = false;
uint32_t total_time = 0;

#define AVIOFFSET 240
unsigned long movi_size = 0;
unsigned long jpeg_size = 0;
const char zero_buf[4] = {0x00, 0x00, 0x00, 0x00};
const char avi_header[AVIOFFSET] PROGMEM ={
   0x52, 0x49, 0x46, 0x46, 0xD8, 0x01, 0x0E, 0x00, 0x41, 0x56, 0x49, 0x20, 0x4C, 0x49, 0x53, 0x54,
  0xD0, 0x00, 0x00, 0x00, 0x68, 0x64, 0x72, 0x6C, 0x61, 0x76, 0x69, 0x68, 0x38, 0x00, 0x00, 0x00,
  0xA0, 0x86, 0x01, 0x00, 0x80, 0x66, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
  0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x40, 0x01, 0x00, 0x00, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4C, 0x49, 0x53, 0x54, 0x84, 0x00, 0x00, 0x00,
  0x73, 0x74, 0x72, 0x6C, 0x73, 0x74, 0x72, 0x68, 0x30, 0x00, 0x00, 0x00, 0x76, 0x69, 0x64, 0x73,
  0x4D, 0x4A, 0x50, 0x47, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x73, 0x74, 0x72, 0x66,
  0x28, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x40, 0x01, 0x00, 0x00, 0xF0, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x18, 0x00, 0x4D, 0x4A, 0x50, 0x47, 0x00, 0x84, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4C, 0x49, 0x53, 0x54,
  0x10, 0x00, 0x00, 0x00, 0x6F, 0x64, 0x6D, 0x6C, 0x64, 0x6D, 0x6C, 0x68, 0x04, 0x00, 0x00, 0x00,
  0x64, 0x00, 0x00, 0x00, 0x4C, 0x49, 0x53, 0x54, 0x00, 0x01, 0x0E, 0x00, 0x6D, 0x6F, 0x76, 0x69,
};

void print_quartet(unsigned long i,File fd){
  fd.write(i % 0x100);  i = i >> 8;   //i /= 0x100;
  fd.write(i % 0x100);  i = i >> 8;   //i /= 0x100;
  fd.write(i % 0x100);  i = i >> 8;   //i /= 0x100;
  fd.write(i % 0x100);
}



ArduCAM myCAM(OV5640, CS);
uint8_t read_fifo_burst(ArduCAM myCAM);

void setup() {
  // put your setup code here, to run once:
  uint8_t vid, pid;
  uint8_t temp;
#if defined(__SAM3X8E__)
  Wire1.begin();
#else
  Wire.begin();
#endif
  Serial.begin(115200);
  Serial.println("ArduCAM Start!");

  // set the CS as an output:
  pinMode(CS, OUTPUT);

  // initialize SPI:
  SPI.begin();
  //Check if the ArduCAM SPI bus is OK
  myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
  temp = myCAM.read_reg(ARDUCHIP_TEST1);
  
 // Serial.println(temp,HEX);
  if (temp != 0x55)
  {
    Serial.println("SPI interface Error!");
    //while(1);
  }
  //Check if the camera module type is OV5640
  myCAM.rdSensorReg16_8(OV5640_CHIPID_HIGH, &vid);
  myCAM.rdSensorReg16_8(OV5640_CHIPID_LOW, &pid);
  if ((vid != 0x56) || (pid != 0x40))
    Serial.println("Can't find OV5640 module!");
  else
    Serial.println("OV5640 detected.");

 //Initialize SD Card
  if (!SD.begin(SD_CS))
  {
    //while (1);    //If failed, stop here
    Serial.println("SD Card Error!");
  }
  else
    Serial.println("SD Card detected.");
  //Change to JPEG capture mode and initialize the OV5640 module
  myCAM.set_format(JPEG);
  myCAM.InitCAM();
  myCAM.set_bit(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);
  myCAM.OV5640_set_JPEG_size(OV5640_640x480);
  myCAM.clear_fifo_flag();
  myCAM.write_reg(ARDUCHIP_FRAMES, FRAMES_NUM);
}

void loop() {
  // put your main code here, to run repeatedly:
  uint8_t temp, temp_last;
  uint32_t length = 0;
  bool is_header = false;
  myCAM.flush_fifo();
  myCAM.clear_fifo_flag();
  //Start capture
  myCAM.start_capture();
  Serial.println("start capture!");
  total_time = millis();
  while ( !myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK)); 
  length = myCAM.read_fifo_length();
  if( length < 0x3FFFFF){
  myCAM.flush_fifo();
  myCAM.clear_fifo_flag();
  //Start capture
  myCAM.start_capture();
  while ( !myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK));
  Serial.println("CAM Capture Done!");
     total_time = millis() - total_time;
      Serial.print("capture total_time used (in miliseconds):");
      Serial.println(total_time, DEC);
  }else{
     Serial.println("CAM Capture Done!");
     total_time = millis() - total_time;
      Serial.print("capture total_time used (in miliseconds):");
      Serial.println(total_time, DEC);
  }
  total_time = millis();
  read_fifo_burst(myCAM);
  total_time = millis() - total_time;
  Serial.print("save video total_time used (in miliseconds):");
  Serial.println(total_time, DEC);
  //Clear the capture done flag
  myCAM.clear_fifo_flag();
  delay(5000);
}

uint8_t read_fifo_burst(ArduCAM myCAM)
{
  
  uint8_t temp, temp_last;
  uint32_t length = 0;
  static int i = 0;
  static int k = 0;
  unsigned long position = 0;
  uint16_t frame_cnt = 0;
  uint8_t remnant = 0;
  char quad_buf[4] = {};
  char str[8];
  File outFile;
  byte buf[256]; 
  length = myCAM.read_fifo_length();
  Serial.print("The fifo length is :");
  Serial.println(length, DEC);
 // Serial.println("writting the data to the SD !");
  if (length >= 0x07fffff) //1M
  {
    Serial.println("Over size.");
    return 0;
  }
  if (length == 0 ) //0 kb
  {
    Serial.println("Size is 0.");
    return 0;
  }
 //Create a avi file
  k = k + 1;
  itoa(k, str, 10);
  strcat(str, ".avi");
  //Open the new file
  outFile = SD.open(str, O_WRITE | O_CREAT | O_TRUNC);
  if (! outFile)
  {
    Serial.println("open file failed");
    while (1);
  }
  //Write AVI Header
  for ( i = 0; i < AVIOFFSET; i++)
  {
    char ch = pgm_read_byte(&avi_header[i]);
    buf[i] = ch;
  }
  outFile.write(buf, AVIOFFSET);
 
  
  myCAM.CS_LOW();
  myCAM.set_fifo_burst();//Set fifo burst mode
  SPI.transfer(0x00);//First byte is 0x00 ,not 0xff
  length--;
  i = 0;
  while ( length-- )
  {
    temp_last = temp;
    temp =  SPI.transfer(0x00);
    // Serial.println(temp,HEX);
    //Read JPEG data from FIFO
     if ( (temp == 0xD9) && (temp_last == 0xFF) ) //If find the end ,break while,
    {
         buf[i++] = temp; //save the last 0XD9
         //Write the remain bytes in the buffer
         myCAM.CS_HIGH();
         outFile.write(buf, i);
         jpeg_size += i;
     
        remnant = (4 - (jpeg_size & 0x00000003)) & 0x00000003;
        jpeg_size = jpeg_size + remnant;
        movi_size = movi_size + jpeg_size;
        if (remnant > 0)
          outFile.write(zero_buf, remnant);
        //Serial.println(movi_size, HEX);
    
        position = outFile.position();
        outFile.seek(position - 4 - jpeg_size);
        print_quartet(jpeg_size, outFile);
        position = outFile.position();
        outFile.seek(position + 6);
        outFile.write("AVI1", 4);
        position = outFile.position();
        outFile.seek(position + jpeg_size - 10);
        is_header = false;
        frame_cnt++;
        myCAM.CS_LOW();
        myCAM.set_fifo_burst();
        i = 0;
    } 
    if (is_header == true)
    { 
       //Write image data to buffer if not full
        if (i < 256)
        buf[i++] = temp;
        else
        {
          //Write 256 bytes image data to file
          myCAM.CS_HIGH();
          outFile.write(buf, 256);
          i = 0;
          buf[i++] = temp;
          myCAM.CS_LOW();
          myCAM.set_fifo_burst();
          jpeg_size += 256;
        }        
    }
    else if ((temp == 0xD8) & (temp_last == 0xFF))
    {
      is_header = true;
      myCAM.CS_HIGH();
      outFile.write("00dc");
      outFile.write(zero_buf, 4);
      i = 0;
      jpeg_size = 0;
      myCAM.CS_LOW();
      myCAM.set_fifo_burst();   
      buf[i++] = temp_last;
      buf[i++] = temp;   
    }
  }
   myCAM.CS_HIGH();
  //Modify the MJPEG header from the beginning of the file
  outFile.seek(4);
  print_quartet(movi_size + 0xd8, outFile);//riff file size
  
  //overwrite hdrl
  unsigned long us_per_frame = 1000000 / FRAME_RATE; //(per_usec); //hdrl.avih.us_per_frame
  outFile.seek(0x20);
  print_quartet(us_per_frame, outFile);
  unsigned long max_bytes_per_sec = movi_size * FRAME_RATE/ frame_cnt; //hdrl.avih.max_bytes_per_sec
  outFile.seek(0x24);
  print_quartet(max_bytes_per_sec, outFile);
  //unsigned long tot_frames = framecnt;    //hdrl.avih.tot_frames
  outFile.seek(0x30);
  print_quartet(max_bytes_per_sec, outFile);
  
  outFile.seek(0x84);
  print_quartet(FRAME_RATE, outFile);// size again
  
  //unsigned long frames =framecnt;// (TOTALFRAMES); //hdrl.strl.list_odml.frames
  outFile.seek(0xe0);
  print_quartet(max_bytes_per_sec, outFile);
  outFile.seek(0xe8);
  print_quartet(movi_size, outFile);// size again
  //Close the file
  outFile.close();
  Serial.println("The movie capture is OK");
  frame_cnt = 0;
}
