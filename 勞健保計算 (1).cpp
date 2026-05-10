#include <bits/stdc++.h>
using namespace std;

struct Item {
    int wage;
    int hour;
    int day;

    int salary;          // 薪資
    int employerLabor;   // 單位勞保費，按日
    int personalLabor;   // 個人勞保費，按日
    int pension;         // 勞退金 6%
    int supplement;      // 補充保費 2.11%

    int fee;             // 單位額外繳納費用：單位勞保 + 勞退 + 補充保費
    int cost;            // 總成本：薪資 + fee
    int net;             // 實領：薪資 - 個人負擔

    int levelAmount;     // 投保級距
};

struct State {
    bool valid = false;

    int cost = 0;
    int salary = 0;
    int fee = 0;
    int net = 0;

    vector<int> picks;
};

// =====================
// 可調整參數
// =====================

constexpr int MIN_HOURLY_WAGE = 196;   // 115 年最低時薪
constexpr int MAX_HOURLY_WAGE = 294;   // 中央大學學生工讀上限：基本工資時薪 1.5 倍

constexpr int MIN_HOURS_PER_DAY = 1;   // 若沒有每日最低工時限制，從 1 小時開始搜尋
constexpr int MAX_HOURS_PER_DAY = 8;

constexpr int MIN_WORK_DAYS = 1;       // 若沒有最低工作天數限制，從 1 天開始搜尋
constexpr int MAX_WORK_DAYS = 44;

constexpr int MAX_MONTHLY_HOURS = 60;  // 學期間每月工讀時數上限；寒暑假可改為 160

constexpr double PENSION_RATE = 0.06;
constexpr double SUPPLEMENT_RATE = 0.0211;

// =====================
// TODO：級距資料仍需整組更新為 115.01 起適用資料
// =====================

vector<int> level{
    1500, 3000, 4500, 6000, 7500, 8700, 9900, 11100,
    12540, 13500, 15840, 16500, 17280, 17880, 19047,
    20008, 21009, 22000, 23100, 24000, 25250, 26400,
    27600, 28590, 28800, 30300, 31800, 33300, 34800,
    36300, 38200, 40100, 42000, 43900, 45800, 48200,
    50600, 53000, 55400, 57800, 60800, 63800, 66800,
    69800, 72800, 76500, 80200, 83900, 87600, 92100,
    96600, 101100, 105600, 110100, 115500, 120900,
    126300, 131700, 137100, 142500, 147900, 150000,
    156400, 162800, 169200, 175600, 182000, 189500,
    197000, 204500, 212000, 219500
};

// 勞保費用：單位負擔合計
vector<int> employerLaborMonthly{
    1004, 1004, 1004, 1004, 1004, 1004, 1004, 1004,
    1129, 1214, 1419, 1477, 1545, 1597, 1700,
    1785, 1872, 1960, 2057, 2135, 2245, 2346,
    2451, 2537, 2556, 2689, 2823, 2955, 3089,
    3221, 3390, 3559, 3728, 3896, 4065, 4067,
    4070, 4072, 4074, 4077, 4080, 4083, 4086,
    4089, 4092, 4092, 4092, 4092, 4092, 4092,
    4092, 4092, 4092, 4092, 4092, 4092, 4092,
    4092, 4092, 4092, 4092, 4092, 4092, 4092,
    4092, 4092, 4092, 4092, 4092, 4092, 4092,
    4092
};

// 勞保費用：個人負擔合計
vector<int> personalLaborMonthly{
    277, 277, 277, 277, 277, 277, 277, 277,
    313, 338, 396, 413, 432, 447, 476,
    500, 525, 550, 577, 600, 632, 660,
    690, 715, 720, 758, 795, 833, 870,
    908, 955, 1002, 1050, 1098, 1145,
    1145, 1145, 1145, 1145, 1145, 1145,
    1145, 1145, 1145, 1145, 1145, 1145,
    1145, 1145, 1145, 1145, 1145, 1145,
    1145, 1145, 1145, 1145, 1145, 1145,
    1145, 1145, 1145, 1145, 1145, 1145,
    1145, 1145, 1145, 1145, 1145, 1145,
    1145
};

int roundMoney(double x) {
    return static_cast<int>(llround(x));
}

int getLevelIndex(int monthlySalary) {
    auto it = lower_bound(level.begin(), level.end(), monthlySalary);

    if (it == level.end()) {
        return static_cast<int>(level.size()) - 1;
    }

    return static_cast<int>(it - level.begin());
}

Item makeItem(int wage, int hour, int day) {
    int salary = wage * hour * day;

    // 延續你原本邏輯：
    // 用「時薪 * 每日工時 * 30」推估月投保級距
    int monthlySalary = wage * hour * 30;
    int idx = getLevelIndex(monthlySalary);

    int employerLabor = roundMoney(employerLaborMonthly[idx] / 30.0 * day);
    int personalLabor = roundMoney(personalLaborMonthly[idx] / 30.0 * day);

    int pension = roundMoney(salary * PENSION_RATE);
    int supplement = roundMoney(salary * SUPPLEMENT_RATE);

    int fee = employerLabor + pension + supplement;
    int cost = salary + fee;
    int net = salary - personalLabor;

    return {
        wage,
        hour,
        day,
        salary,
        employerLabor,
        personalLabor,
        pension,
        supplement,
        fee,
        cost,
        net,
        level[idx]
    };
}

vector<Item> buildItems(int budget) {
    vector<Item> items;

    for (int wage = MIN_HOURLY_WAGE; wage <= MAX_HOURLY_WAGE; ++wage) {
        for (int hour = MIN_HOURS_PER_DAY; hour <= MAX_HOURS_PER_DAY; ++hour) {
            for (int day = MIN_WORK_DAYS; day <= MAX_WORK_DAYS; ++day) {
                // 中央大學學生工讀助學辦法：學期間每月工讀時數以不超過 60 小時為原則
                if (hour * day > MAX_MONTHLY_HOURS) continue;

                Item item = makeItem(wage, hour, day);

                if (item.cost <= budget) {
                    items.push_back(item);
                }
            }
        }
    }

    return items;
}

// =====================
// 方案 A：剩餘的錢最少
// =====================
//
// 主要目標：預算剩餘最少
// 次要目標：實領較高
// 再次目標：繳納費用較少
//
bool betterLessLeftover(const State& a, const State& b, int budget) {
    if (!b.valid) return true;

    int leftA = budget - a.cost;
    int leftB = budget - b.cost;

    if (leftA != leftB) return leftA < leftB;
    if (a.net != b.net) return a.net > b.net;
    if (a.fee != b.fee) return a.fee < b.fee;

    return a.picks.size() < b.picks.size();
}

// =====================
// 方案 B：繳納費用最少
// =====================
//
// 注意：
// 如果只比 fee 最少，最佳解通常會變成最低薪資、最低工時、最低天數。
// 所以這裡定義成：
//
// 主要目標：薪資最高
// 次要目標：繳納費用最少
// 再次目標：預算剩餘較少
//
bool betterLessFee(const State& a, const State& b, int budget) {
    if (!b.valid) return true;

    if (a.salary != b.salary) return a.salary > b.salary;
    if (a.fee != b.fee) return a.fee < b.fee;

    int leftA = budget - a.cost;
    int leftB = budget - b.cost;

    if (leftA != leftB) return leftA < leftB;
    if (a.net != b.net) return a.net > b.net;

    return a.picks.size() < b.picks.size();
}

using CompareFunc = function<bool(const State&, const State&, int)>;

State solve(
    int budget,
    int maxParts,
    const vector<Item>& items,
    CompareFunc better
) {
    vector<State> cur(budget + 1);
    vector<State> next(budget + 1);

    cur[0].valid = true;

    State best;
    best.valid = false;

    for (int used = 0; used < maxParts; ++used) {
        next = cur;

        for (int currentCost = 0; currentCost <= budget; ++currentCost) {
            if (!cur[currentCost].valid) continue;

            for (int i = 0; i < static_cast<int>(items.size()); ++i) {
                const Item& item = items[i];

                int nextCost = currentCost + item.cost;
                if (nextCost > budget) continue;

                State candidate = cur[currentCost];

                candidate.valid = true;
                candidate.cost += item.cost;
                candidate.salary += item.salary;
                candidate.fee += item.fee;
                candidate.net += item.net;
                candidate.picks.push_back(i);

                if (!next[nextCost].valid || better(candidate, next[nextCost], budget)) {
                    next[nextCost] = candidate;
                }
            }
        }

        cur.swap(next);
    }

    for (const State& st : cur) {
        if (!st.valid || st.picks.empty()) continue;

        if (!best.valid || better(st, best, budget)) {
            best = st;
        }
    }

    return best;
}

void printItem(const Item& item) {
    cout << "時薪: " << item.wage
         << ", 每日工作時數: " << item.hour
         << ", 天數: " << item.day
         << ", 投保級距: " << item.levelAmount << '\n';

    cout << "薪資: " << item.salary
         << ", 單位勞保費: " << item.employerLabor
         << ", 勞退金: " << item.pension
         << ", 補充保費: " << item.supplement
         << ", 繳納費用: " << item.fee
         << ", 總成本: " << item.cost
         << ", 個人負擔: " << item.personalLabor
         << ", 實領: " << item.net
         << "\n\n";
}

void printSolution(
    const string& title,
    const State& st,
    const vector<Item>& items,
    int budget
) {
    cout << "========== " << title << " ==========\n";

    if (!st.valid) {
        cout << "找不到可行解\n\n";
        return;
    }

    for (int idx : st.picks) {
        printItem(items[idx]);
    }

    cout << "合計薪資: " << st.salary << '\n';
    cout << "合計繳納費用: " << st.fee << '\n';
    cout << "合計總成本: " << st.cost << '\n';
    cout << "合計實領: " << st.net << '\n';
    cout << "預算剩餘: " << budget - st.cost << '\n';
    cout << "使用組數: " << st.picks.size() << "\n\n";
}

int main() {
    int budget = 21323;   // 預算
    int maxParts = 1;    // 最多分成幾種類型

    vector<Item> items = buildItems(budget);

    cout << "可行組合數量: " << items.size() << "\n\n";

    State lessLeftover = solve(
        budget,
        maxParts,
        items,
        betterLessLeftover
    );

    State lessFee = solve(
        budget,
        maxParts,
        items,
        betterLessFee
    );

    printSolution(
        "方案 A：剩餘的錢最少",
        lessLeftover,
        items,
        budget
    );

    printSolution(
        "方案 B：繳納費用最少",
        lessFee,
        items,
        budget
    );

    return 0;
}