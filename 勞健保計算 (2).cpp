#include <iostream>
#include <vector>
#include <cmath>
#include <climits>
#include <iomanip>
using namespace std;

/*
  工讀勞健保成本估算
  - 時薪範圍 196~294
  - 每月工時上限檢查
  - DP 預算內做效益最佳化（分別計算拆一筆/拆兩筆）
  - 級距資料表一致性檢查
*/

constexpr int MIN_HOURLY_WAGE = 196;
constexpr int MAX_HOURLY_WAGE = 294;
constexpr int MIN_HOURS_PER_DAY = 1;
constexpr int MAX_HOURS_PER_DAY = 8;
constexpr int MIN_WORK_DAYS = 1;
constexpr int MAX_PARTS = 2;  // 拆 1~2 筆

// 學期間上限 60；寒暑假改 160
constexpr int MAX_MONTHLY_HOURS = 60;

constexpr double PENSION_RATE = 0.06;
constexpr double SUPPLEMENT_RATE = 0.0211;

vector<int> level{
    1500, 3000, 4500, 6000, 7500, 8700, 9900, 11100, 12540, 13500,
    15840, 16500, 17280, 17880, 19047, 20008, 21009, 22000, 23100, 24000,
    25250, 26400, 27600, 28590, 29500, 30300, 31800, 33300, 34800, 36300,
    38200, 40100, 42000, 43900, 45800
};

vector<int> employerLaborMonthly{
    972, 972, 972, 972, 972, 972, 972, 972, 1097, 1182,
    1386, 1444, 1512, 1564, 1666, 1751, 1838, 1925, 2022, 2100,
    2209, 2310, 2415, 2501, 2582, 2651, 2783, 2914, 3045, 3176,
    3342, 3509, 3675, 3841, 4008
};

vector<int> personalLaborMonthly{
    277, 277, 277, 277, 277, 277, 277, 277, 313, 338,
    396, 413, 432, 447, 476, 500, 525, 550, 577, 600,
    632, 660, 690, 715, 738, 758, 795, 833, 870, 908,
    955, 1002, 1050, 1098, 1145
};

struct CalcResult {
    int totalCost;
    int netPay;
    int grossPay;
    int employerLabor;
    int pension;
    int supplement;
    int personalLabor;
};

struct Option {
    int wage;
    int hour;
    int day;
    int cost;
    int net;
};

int laborPension(int grossPay) {
    return static_cast<int>(round(grossPay * PENSION_RATE));
}

int supplementalFee(int grossPay) {
    return static_cast<int>(round(grossPay * SUPPLEMENT_RATE));
}

int findLevelIndex(int monthlySalaryEstimate) {
    int idx = 0;
    for (idx = 0; idx < static_cast<int>(level.size()) - 1 && monthlySalaryEstimate > level[idx]; idx++);
    return idx;
}

bool validateTables() {
    if (!(level.size() == employerLaborMonthly.size() &&
          level.size() == personalLaborMonthly.size())) {
        cerr << "錯誤：level / employerLaborMonthly / personalLaborMonthly 長度不一致，請整組同步更新。\n";
        return false;
    }

    // 允許相鄰重複值（非遞減）；若遞減則視為資料錯誤
    for (size_t i = 1; i < level.size(); ++i) {
        if (level[i] < level[i - 1]) {
            cerr << "錯誤：level 級距在 index " << i << " 出現遞減，請依 115 年正式資料修正。\n";
            return false;
        }
    }
    return true;
}

CalcResult calculate(int wage, int hour, int day) {
    int monthlySalaryEstimate = wage * hour * 30;  // 沿用原始月薪估算方式
    int idx = findLevelIndex(monthlySalaryEstimate);

    int grossPay = wage * hour * day;
    int employerLabor = static_cast<int>(round(employerLaborMonthly[idx] / 30.0 * day));
    int pension = laborPension(grossPay);
    int supplement = supplementalFee(grossPay);
    int personalLabor = static_cast<int>(round(personalLaborMonthly[idx] / 30.0 * day));

    int totalCost = grossPay + employerLabor + pension + supplement;
    int netPay = grossPay - personalLabor;

    return {totalCost, netPay, grossPay, employerLabor, pension, supplement, personalLabor};
}

void printDetail(int wage, int hour, int day) {
    CalcResult r = calculate(wage, hour, day);
    cout << "--------------------------------------------------\n";
    cout << "工讀方案明細\n";
    cout << "--------------------------------------------------\n";
    cout << left
         << setw(18) << "時薪" << ": " << wage << "\n"
         << setw(18) << "每日工作時數" << ": " << hour << "\n"
         << setw(18) << "天數" << ": " << day << "\n"
         << setw(18) << "總薪水" << ": " << r.grossPay << "\n"
         << setw(18) << "勞保費(雇主)" << ": " << r.employerLabor << "\n"
         << setw(18) << "勞退金(6%)" << ": " << r.pension << "\n"
         << setw(18) << "補充保費(2.11%)" << ": " << r.supplement << "\n"
         << setw(18) << "個人負擔" << ": " << r.personalLabor << "\n"
         << setw(18) << "實領" << ": " << r.netPay << "\n"
         << setw(18) << "總成本" << ": " << r.totalCost << "\n";
}

vector<Option> buildOptions(int budget) {
    // 同成本只保留「實領最高」的方案，減少 DP 狀態轉移量
    vector<Option> bestByCost(budget + 1, {-1, -1, -1, -1, INT_MIN});

    for (int wage = MIN_HOURLY_WAGE; wage <= MAX_HOURLY_WAGE; ++wage) {
        for (int hour = MIN_HOURS_PER_DAY; hour <= MAX_HOURS_PER_DAY; ++hour) {
            int maxDaysByMonthlyHours = MAX_MONTHLY_HOURS / hour;
            for (int day = MIN_WORK_DAYS; day <= maxDaysByMonthlyHours; ++day) {
                if (hour * day > MAX_MONTHLY_HOURS) continue;

                CalcResult r = calculate(wage, hour, day);
                if (r.totalCost > budget) continue;

                Option candidate{wage, hour, day, r.totalCost, r.netPay};
                if (candidate.net > bestByCost[candidate.cost].net) {
                    bestByCost[candidate.cost] = candidate;
                }
            }
        }
    }

    vector<Option> options;
    for (int c = 1; c <= budget; ++c) {
        if (bestByCost[c].cost != -1) options.push_back(bestByCost[c]);
    }
    return options;
}

int main() {
    if (!validateTables()) return 1;

    int budget = 12000;
    vector<Option> options = buildOptions(budget);

    if (options.empty()) {
        cout << "在目前條件下沒有可用的單筆方案。\n";
        return 0;
    }

    const int NEG_INF = INT_MIN / 4;
    // dpNet[k][b]：恰好拆 k 筆、用到 b 預算時的最大總實領
    vector<vector<int>> dpNet(MAX_PARTS + 1, vector<int>(budget + 1, NEG_INF));
    vector<vector<int>> prevBudget(MAX_PARTS + 1, vector<int>(budget + 1, -1));
    vector<vector<int>> prevOption(MAX_PARTS + 1, vector<int>(budget + 1, -1));

    dpNet[0][0] = 0;

    // 每次多加一筆，最多加到 MAX_PARTS
    for (int k = 1; k <= MAX_PARTS; ++k) {
        for (int b = 0; b <= budget; ++b) {
            if (dpNet[k - 1][b] == NEG_INF) continue;
            for (int i = 0; i < static_cast<int>(options.size()); ++i) {
                int nb = b + options[i].cost;
                if (nb > budget) continue;

                int candNet = dpNet[k - 1][b] + options[i].net;
                if (candNet > dpNet[k][nb]) {
                    dpNet[k][nb] = candNet;
                    prevBudget[k][nb] = b;
                    prevOption[k][nb] = i;
                }
            }
        }
    }

    cout << "==================================================\n";
    cout << "115 年中央大學學生工讀／勞健保成本估算（DP）\n";
    cout << "==================================================\n";
    cout << left
         << setw(18) << "時薪範圍" << ": " << MIN_HOURLY_WAGE << " ~ " << MAX_HOURLY_WAGE << "\n"
         << setw(18) << "每日工時範圍" << ": " << MIN_HOURS_PER_DAY << " ~ " << MAX_HOURS_PER_DAY << "\n"
         << setw(18) << "每月工時上限" << ": " << MAX_MONTHLY_HOURS << "\n"
         << setw(18) << "最多拆分筆數" << ": " << MAX_PARTS << "\n"
         << setw(18) << "預算" << ": " << budget << "\n"
         << setw(18) << "方案數(去重後)" << ": " << options.size() << "\n";

    auto printBestForParts = [&](int parts) {
        int bestUsed = -1;
        int bestNet = NEG_INF;
        for (int b = 0; b <= budget; ++b) {
            if (dpNet[parts][b] == NEG_INF) continue;
            if (dpNet[parts][b] > bestNet ||
                (dpNet[parts][b] == bestNet && b > bestUsed)) {
                bestNet = dpNet[parts][b];
                bestUsed = b;
            }
        }

        cout << "==================================================\n";
        cout << "拆分 " << parts << " 筆最佳解\n";
        cout << "==================================================\n";
        if (bestUsed == -1) {
            cout << "在目前條件下找不到可行組合。\n";
            return;
        }

        vector<Option> selected;
        int cur = bestUsed;
        int k = parts;
        while (k > 0 && cur >= 0 && prevBudget[k][cur] != -1 && prevOption[k][cur] != -1) {
            selected.push_back(options[prevOption[k][cur]]);
            cur = prevBudget[k][cur];
            --k;
        }

        for (const auto& op : selected) {
            printDetail(op.wage, op.hour, op.day);
        }

        cout << "--------------------------------------------------\n";
        cout << left
             << setw(18) << "拆分筆數" << ": " << parts << "\n"
             << setw(18) << "已用預算" << ": " << bestUsed << "\n"
             << setw(18) << "剩餘預算" << ": " << (budget - bestUsed) << "\n"
             << setw(18) << "總實領" << ": " << bestNet << "\n";
    };

    printBestForParts(1);
    printBestForParts(2);

    return 0;
}
