#pragma once

#include <QString>

#include <functional>
#include <vector>

// 安装、卸载按一步一步做；一步没成，已经做了的倒着撤回（undo），机器回到开始以前的样子。
// run 在后台线程上跑，返回空字符串表示成了，否则是给人看的原因；progress 报这一步做了多少（0..1）
struct SetupStep
{
    QString name; // 日志里用
    double weight = 1; // 占整个进度的多少（相对其他步）
    std::function<QString(const std::function<void(double)> &progress)> run;
    std::function<void()> undo; // 可以没有：这一步不用撤（比如只是停掉旧的程序）
};

// 按顺序做完 steps：返回空字符串表示都成了；没成时撤回做过的，返回原因。total 报整个的进度（0..1）
QString runSetupSteps(const std::vector<SetupStep> &steps, const std::function<void(double total)> &total);
