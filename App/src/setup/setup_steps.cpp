#include "setup_steps.h"

#include <QtGlobal>

#include <numeric>

QString runSetupSteps(const std::vector<SetupStep> &steps, const std::function<void(double total)> &total)
{
    const double all = std::accumulate(steps.cbegin(), steps.cend(), 0.0,
                                       [](double sum, const SetupStep &step) { return sum + step.weight; });
    double done = 0;
    for (std::size_t i = 0; i < steps.size(); ++i) {
        const SetupStep &step = steps[i];
        qInfo("安装：%s", qUtf8Printable(step.name));
        const QString error = step.run([&](double part) { total((done + step.weight * part) / all); });
        if (error.isEmpty()) {
            done += step.weight;
            total(done / all);
            continue;
        }
        qWarning("安装：%s 没成：%s，撤回已经做了的", qUtf8Printable(step.name), qUtf8Printable(error));
        for (std::size_t back = i; back-- > 0;) {
            if (steps[back].undo)
                steps[back].undo();
        }
        return error;
    }
    return {};
}
