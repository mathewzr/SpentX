#include <fstream>
#include <iostream>
#include <random>

#include "spent/core.hpp"

using namespace spent;

static int failures = 0;
#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
            ++failures;                                                             \
        }                                                                           \
    } while (0)

int main() {
    long long c = 0;
    CHECK(parse_money("85", c) && c == 8500);
    CHECK(parse_money("12.5", c) && c == 1250);
    CHECK(parse_money("1,200.50", c) && c == 120050);
    CHECK(parse_money("0.05", c) && c == 5);
    CHECK(!parse_money("", c));
    CHECK(!parse_money("abc", c));
    CHECK(!parse_money("1.234", c));
    CHECK(!parse_money("-5", c));
    CHECK(!parse_money("0", c));
    CHECK(!parse_money("1.2.3", c));

    CHECK(format_money(120050) == "1,200.50");
    CHECK(format_money(5) == "0.05");
    CHECK(format_money(100000000) == "1,000,000.00");
    CHECK(format_money(99900) == "999.00");

    CHECK(valid_date("2026-02-28"));
    CHECK(!valid_date("2026-02-29"));
    CHECK(valid_date("2028-02-29"));
    CHECK(!valid_date("2026-13-01"));
    CHECK(!valid_date("26-1-1"));
    CHECK(resolve_date("garbage").empty());
    CHECK(valid_date(resolve_date("today")) && valid_date(resolve_date("yesterday")));
    CHECK(resolve_month("2026-10") == "2026-10");
    CHECK(resolve_month("all") == "all");
    CHECK(resolve_month("2026-13").empty());
    CHECK(resolve_month("last").size() == 7);
    CHECK(month_label("2026-10") == "October 2026");

    CHECK(normalize_category("  FooD ") == "food");

    // csv round trip with awkward notes
    std::string nasty = "lunch, with \"quotes\" and commas";
    auto f = csv_split("3," + csv_escape(nasty) + ",x");
    CHECK(f.size() == 3 && f[1] == nasty);

    // storage round trip + filters + totals
    std::random_device rd;
    fs::path dir = fs::temp_directory_path() / ("spent_test_" + std::to_string(rd()));
    fs::create_directories(dir);
    std::vector<Expense> items = {
        {1, "2026-10-01", 120050, "rent", ""},
        {2, "2026-10-05", 8500, "food", nasty},
        {3, "2026-10-09", 4000, "food", "dinner"},
        {4, "2026-09-30", 999, "food", "old"},
    };
    save_expenses(dir / "e.csv", items);
    auto back = load_expenses(dir / "e.csv");
    CHECK(back.size() == 4 && back[1].note == nasty && back[0].cents == 120050);

    auto oct = filter(back, "2026-10", "");
    CHECK(oct.size() == 3);
    CHECK(filter(back, "all", "food").size() == 3);
    CHECK(total(oct) == 132550);
    auto cats = totals_by_category(oct);
    CHECK(cats.size() == 2 && cats[0].category == "rent" && cats[1].cents == 12500);

    std::map<std::string, long long> b = {{"food", 300000}, {"fun", 50000}};
    save_budgets(dir / "b.csv", b);
    CHECK(load_budgets(dir / "b.csv") == b);
    CHECK(load_expenses(dir / "missing.csv").empty());
    fs::remove_all(dir);

    if (failures == 0) std::cout << "All tests passed\n";
    return failures == 0 ? 0 : 1;
}
