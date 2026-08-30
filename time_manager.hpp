#ifndef TIME_MANAGER_HPP
#define TIME_MANAGER_HPP

#include <chrono>
#include <algorithm>


namespace engine {


class TimeManager
{

private:

    std::chrono::steady_clock::time_point start;

    int limit_ms = 30000;


public:

    void startTimer(int ms)
    {
        limit_ms = ms;

        start =
        std::chrono::steady_clock::now();
    }



    bool isTimeUp() const
    {
        auto now =
        std::chrono::steady_clock::now();


        int elapsed =
        std::chrono::duration_cast<
            std::chrono::milliseconds
        >(now-start).count();


        return elapsed >= limit_ms;
    }

    static int calculateTime(
    int remaining,
    int movestogo
    )
    {
        if(remaining <= 50)
            return 10;


        int time;


        if(movestogo > 0)
        {
            // 平均分配剩余时间
            time = remaining / movestogo;
        }
        else
        {
            // 没有movestogo，假设30步
            time = remaining / 30;
        }


        // 最大不能超过剩余时间50%
        time = std::min(
            time,
            remaining / 2
        );


        // 防止过快
        time = std::max(
            time,
            20
        );


        return time;
    }


};


inline TimeManager timer;


}


#endif