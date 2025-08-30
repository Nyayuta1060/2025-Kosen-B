#include "mbed.h"
#include "C620.hpp"
#include "PID_new.hpp"
#include <array>

CAN can(PC_6,PC_7,1e6);
PidGain gain ={0.001, 0.001, 0.0};
BufferedSerial pc(USBTX,USBRX,115200);
dji::C620 robomas(PD_6,PD_5);

constexpr int robomas_amount = 8;

std::array<Pid, robomas_amount> pid = {
  Pid({gain, -1, 1}),
  Pid({gain, -1, 1}),
  Pid({gain, -1, 1}),
  Pid({gain, -1, 1}),
  Pid({gain, -1, 1}),
  Pid({gain, -1, 1}),
  Pid({gain, -1, 1}),
  Pid({gain, -1, 1})
};

constexpr int can_id[3] = {1,2,4};
int16_t pwm1[4] = {0};
int16_t pwm2[4] = {0};
int16_t pwm3[4] = {0};

float duration_to_sec(const std::chrono::duration<float> &duration);

struct PS5
{
    int8_t lstick_x = 0;
    int8_t lstick_y = 0;
    int8_t rstick_x = 0;
    int8_t rstick_y = 0;
    uint8_t l2 = 0;
    uint8_t r2 = 0;

    bool right = 0;
    bool up = 0;
    bool left = 0;
    bool down = 0;
    bool circle = 0;
    bool triangle = 0;
    bool square = 0;
    bool cross = 0;
    bool l1 = 0;
    bool r1 = 0;
    bool l3 = 0;
    bool r3 = 0;
    bool option = 0;
    bool share = 0;

    void parse(CANMessage msg)
    {
        switch (msg.id)
        {
            case 50:
            lstick_x = msg.data[0];
            lstick_y = msg.data[1];
            rstick_x = msg.data[2];
            rstick_y = msg.data[3];
            l2 = msg.data[4];
            r2 = msg.data[5];
            break;

            case 51:
            right = msg.data[0] >> 3 & 1;
            up = msg.data[0] >> 2 & 1;
            left = msg.data[0] >> 1 & 1;
            down = msg.data[0] & 1;
            circle = msg.data[1] >> 3 & 1;
            triangle = msg.data[1] >> 2 & 1;
            square = msg.data[1] >> 1 & 1;
            cross = msg.data[1] & 1;
            l1 = msg.data[2];
            r1 = msg.data[3];
            l3 = msg.data[4];
            r3 = msg.data[5];
            option = msg.data[6];
            share = msg.data[7];
            break;
        }
    }

    bool read(CAN& can)
    {
        CANMessage msg;
        if (can.read(msg); msg.id == 50 || msg.id == 51)
        {
            parse(msg);
            return true;
        }
        return false;
    }
};

int main(){
  PS5 ps5;
  int16_t robomas_rpm[8] = {0};

  constexpr int main_blocker_speed = 10000;
  constexpr int sub_blocker_speed = 10000;
  constexpr int panda_arm_speed = 10000;
  constexpr int panda_lift_speed = 10000;

  for (int i = 0; i < robomas_amount; ++i)
  {
      pid[i].reset();
  }

  while(1){

    static bool pre_cross = 0;
    static bool pre_circle = 0;
    static bool pre_square = 0;
    static bool pre_triangle = 0;
    static bool pre_left = 0;
    static bool pre_right = 0;
    static bool pre_up = 0;
    static bool pre_down = 0;
    
    auto now = HighResClock::now();
    static auto pre = now;
    robomas.read_data();

    //ボタンの入力処理
    if(ps5.read(can)){
      if(ps5.circle == 1 && pre_circle == 0 && ps5.cross == 0){
        pwm1[0] = main_blocker_speed;
      }else if(ps5.cross == 1 && pre_cross == 0 && ps5.circle == 0){
        pwm1[0] = -main_blocker_speed;
      }else if(ps5.circle == 0 && ps5.cross == 0){
        pwm1[0] = 0;
      }

      if(ps5.triangle == 1 && pre_triangle == 0 && ps5.square == 0){
        pwm1[1] = sub_blocker_speed;
      }else if(ps5.square == 1 && pre_square == 0 && ps5.triangle == 0){
        pwm1[1] = -sub_blocker_speed;
      }else if(ps5.triangle == 0 && ps5.square == 0){
        pwm1[1] = 0;
      }

      if(ps5.left == 1 && pre_left == 0 && ps5.right == 0){
        pwm1[2] = panda_arm_speed;
      }else if(ps5.right == 1 && pre_right == 0 && ps5.left == 0){
        pwm1[2] = -panda_arm_speed;
      }else if(ps5.right == 0 && ps5.left == 0){
        pwm1[2] = 0;
      }

      if(ps5.up == 1 && pre_up == 0 && ps5.down == 0){
        pwm[3] = panda_lift_speed;
      }else if(ps5.down == 1 && pre_down == 0 && ps5.up == 0){
        pwm[3] = -panda_lift_speed;
      }else if(ps5.up == 0 && ps5.down == 0){
        pwm[3] = 0;
      }

      pre_right = ps5.right;
      pre_left = ps5.left;
      pre_up = ps5.up;
      pre_down = ps5.down;
      pre_circle = ps5.circle;
      pre_cross = ps5.cross;
      pre_triangle = ps5.triangle
      pre_square = ps5.square
  }

    //CAN送信処理

    if(now - pre > 10ms){
      float elapsed = duration_to_sec(now - pre);

      for (int i = 0; i < robomas_amount; i++){
        int motor_dps = robomas.get_rpm(i + 1);
        const float percent = pid[i].calc(robomas_rpm[i], motor_dps, elapsed);

        robomas.set_output_percent(percent, i + 1);
        }

      CANMessage msg1(can_id[0], (const uint8_t *)&pwm1, 8);
      CANMessage msg2(can_id[1], (const uint8_t *)&pwm2, 8);
      CANMessage msg3(can_id[2], (const uint8_t *)&pwm3, 8);
      can.write(msg1);
      can.write(msg2);
      can.write(msg3);

      robomas.write();
      pre = now;
    }
  }
}

float duration_to_sec(const std::chrono::duration<float> &duration)
{
    return duration.count();
}