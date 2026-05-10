#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

enum class TermType {
    Semester,
    Vacation
};

struct Policy {
    TermType term = TermType::Semester;
    bool continuousEmployment = true;
    bool includeUnitSupplement = true;
};

struct Item {
    int wage = 0;
    int hour = 0;
    int day = 0;

    int salary = 0;
    int insuredSalary = 0;
    int employerLabor = 0;
    int personalLabor = 0;
    int pension = 0;
    int supplement = 0;

    int fee = 0;
    int cost = 0;
    int net = 0;
};

struct State {
    bool valid = false;
    int cost = 0;
    int salary = 0;
    int fee = 0;
    int net = 0;
    vector<int> picks;
};

constexpr int MIN_HOURLY_WAGE = 196;     // 115/2026 legal minimum hourly wage
constexpr int MAX_HOURLY_WAGE = 294;     // school guideline: <= 1.5x minimum
constexpr int MIN_HOURS_PER_DAY = 1;
constexpr int MAX_HOURS_PER_DAY = 8;     // Labor Standards Act normal daily cap
constexpr int MIN_WORK_DAYS = 1;
constexpr int MAX_WORK_DAYS = 31;        // realistic monthly calendar span
constexpr int SEMESTER_MAX_MONTH_HOURS = 60;
constexpr int VACATION_MAX_MONTH_HOURS = 160;

constexpr double PENSION_RATE = 0.06;
constexpr double SUPPLEMENT_RATE = 0.0211;

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

int getMonthlyHourCap(TermType term) {
    return (term == TermType::Semester) ? SEMESTER_MAX_MONTH_HOURS : VACATION_MAX_MONTH_HOURS;
}

int getLevelIndex(int monthlySalary) {
    auto it = lower_bound(level.begin(), level.end(), monthlySalary);
    if (it == level.end()) {
        return static_cast<int>(level.size()) - 1;
    }
    return static_cast<int>(it - level.begin());
}

Item makeItem(int wage, int hour, int day, const Policy& policy) {
    const int salary = wage * hour * day;

    // Keep monthly insured wage based on agreed monthly workload.
    const int monthlySalary = wage * hour * 30;
    const int idx = getLevelIndex(monthlySalary);
    const int insuredSalary = level[idx];

    const int employerLabor = policy.continuousEmployment
        ? employerLaborMonthly[idx]
        : roundMoney(employerLaborMonthly[idx] / 30.0 * day);

    const int personalLabor = policy.continuousEmployment
        ? personalLaborMonthly[idx]
        : roundMoney(personalLaborMonthly[idx] / 30.0 * day);

    // Legal minimum is 6%; for continuous employment, use insured monthly wage as the base.
    const int pensionBase = policy.continuousEmployment ? insuredSalary : salary;
    const int pension = roundMoney(pensionBase * PENSION_RATE);

    int supplement = 0;
    if (policy.includeUnitSupplement) {
        // Unit supplement fee follows:
        // (salary total - insured salary total) * rate, floored at 0.
        const int supplementBase = max(0, salary - insuredSalary);
        supplement = roundMoney(supplementBase * SUPPLEMENT_RATE);
    }

    const int fee = employerLabor + pension + supplement;
    const int cost = salary + fee;
    const int net = salary - personalLabor;

    return {
        wage,
        hour,
        day,
        salary,
        insuredSalary,
        employerLabor,
        personalLabor,
        pension,
        supplement,
        fee,
        cost,
        net
    };
}

vector<Item> buildItems(int budget, const Policy& policy) {
    vector<Item> items;
    const int maxMonthHours = getMonthlyHourCap(policy.term);

    for (int wage = MIN_HOURLY_WAGE; wage <= MAX_HOURLY_WAGE; ++wage) {
        for (int hour = MIN_HOURS_PER_DAY; hour <= MAX_HOURS_PER_DAY; ++hour) {
            for (int day = MIN_WORK_DAYS; day <= MAX_WORK_DAYS; ++day) {
                if (hour * day > maxMonthHours) continue;

                const Item item = makeItem(wage, hour, day, policy);
                if (item.cost <= budget) {
                    items.push_back(item);
                }
            }
        }
    }

    return items;
}

bool betterLessLeftover(const State& a, const State& b, int budget) {
    if (!b.valid) return true;

    const int leftA = budget - a.cost;
    const int leftB = budget - b.cost;
    if (leftA != leftB) return leftA < leftB;
    if (a.net != b.net) return a.net > b.net;
    if (a.fee != b.fee) return a.fee < b.fee;
    return a.picks.size() < b.picks.size();
}

bool betterLessFee(const State& a, const State& b, int budget) {
    if (!b.valid) return true;

    if (a.salary != b.salary) return a.salary > b.salary;
    if (a.fee != b.fee) return a.fee < b.fee;

    const int leftA = budget - a.cost;
    const int leftB = budget - b.cost;
    if (leftA != leftB) return leftA < leftB;
    if (a.net != b.net) return a.net > b.net;
    return a.picks.size() < b.picks.size();
}

using CompareFunc = function<bool(const State&, const State&, int)>;

State solve(int budget, int maxParts, const vector<Item>& items, CompareFunc better) {
    vector<State> cur(budget + 1);
    vector<State> next(budget + 1);
    cur[0].valid = true;

    State best;

    for (int used = 0; used < maxParts; ++used) {
        next = cur;

        for (int currentCost = 0; currentCost <= budget; ++currentCost) {
            if (!cur[currentCost].valid) continue;

            for (int i = 0; i < static_cast<int>(items.size()); ++i) {
                const Item& item = items[i];
                const int nextCost = currentCost + item.cost;
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
    cout << "wage=" << item.wage
         << ", hour/day=" << item.hour
         << ", workDays=" << item.day
         << ", insuredSalary=" << item.insuredSalary << '\n';

    cout << "salary=" << item.salary
         << ", employerLabor=" << item.employerLabor
         << ", pension=" << item.pension
         << ", supplement=" << item.supplement
         << ", fee=" << item.fee
         << ", cost=" << item.cost
         << ", personalLabor=" << item.personalLabor
         << ", net=" << item.net
         << "\n\n";
}

void printSolution(const string& title, const State& st, const vector<Item>& items, int budget) {
    cout << "========== " << title << " ==========\n";

    if (!st.valid) {
        cout << "No feasible solution.\n\n";
        return;
    }

    for (int idx : st.picks) {
        printItem(items[idx]);
    }

    cout << "totalSalary=" << st.salary << '\n';
    cout << "totalFee=" << st.fee << '\n';
    cout << "totalCost=" << st.cost << '\n';
    cout << "totalNet=" << st.net << '\n';
    cout << "budgetLeft=" << budget - st.cost << '\n';
    cout << "selectedCount=" << st.picks.size() << "\n\n";
}

int main() {
    const int budget = 21323;
    const int maxParts = 1;

    // School/legal rule switches:
    // - term: semester(60h) / vacation(160h)
    // - continuousEmployment: if true, labor insurance and pension use full-month logic
    // - includeUnitSupplement: if true, use unit supplement formula by salary gap
    Policy policy;
    policy.term = TermType::Semester;
    policy.continuousEmployment = true;
    policy.includeUnitSupplement = true;

    const vector<Item> items = buildItems(budget, policy);
    cout << "candidateCount=" << items.size() << "\n\n";

    const State lessLeftover = solve(budget, maxParts, items, betterLessLeftover);
    const State lessFee = solve(budget, maxParts, items, betterLessFee);

    printSolution("A: minimize leftover budget", lessLeftover, items, budget);
    printSolution("B: maximize salary then minimize fee", lessFee, items, budget);

    return 0;
}
