#pragma once
constexpr int TIMER_GROUP_0 = 0, TIMER_0 = 0, TIMER_PAUSE = 0;
void timer_group_set_counter_enable_in_isr(int group, int timer, int enabled);
