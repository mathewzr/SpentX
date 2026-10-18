#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>

#include "spent/core.hpp"

using namespace spent;

static const char* kVersion = "0.1.0";

static const char* kUsage = R"HELP(spent - a tiny expense tracker for the command line

Usage:
  spent add AMOUNT CATEGORY [note...] [-d DATE]   log an expense
  spent list [-m MONTH] [-c CATEGORY]             show expenses
  spent summary [-m MONTH]                        totals per category (+ budgets)
  spent remove ID                                 delete an expense
  spent budget                                    show monthly budgets
  spent budget set CATEGORY AMOUNT                set a monthly budget
  spent budget rm CATEGORY                        remove a budget
  spent where                                     show where your data is stored

DATE   today (default), yesterday, or YYYY-MM-DD
MONTH  this (default), last, YYYY-MM, or all

Examples:
  spent add 85 food lunch with team
  spent add 1200.50 rent -d 2026-10-01
  spent budget set food 3000
  spent summary
  spent list -m last -c food

Environment:
  SPENT_DIR       where data is kept (default: ~/.spent)
  SPENT_CURRENCY  text shown before amounts, e.g. "$" or "THB "
)HELP";

static fs::path data_dir() {
    if (const char* d = std::getenv("SPENT_DIR")) return d;
    const char* home = std::getenv("HOME");
    if (!home) home = std::getenv("USERPROFILE");
    if (!home) throw std::runtime_error("cannot find your home folder; set SPENT_DIR");
    return fs::path(home) / ".spent";
}

static std::string money(long long cents) {
    const char* cur = std::getenv("SPENT_CURRENCY");
    return std::string(cur ? cur : "") + format_money(cents);
}

static std::string repeat(char c, int n) { return std::string(std::max(0, n), c); }

static int next_id(const std::vector<Expense>& items) {
    int id = 0;
    for (const auto& e : items) id = std::max(id, e.id);
    return id + 1;
}

// ---------- commands ----------

static int cmd_add(const std::vector<std::string>& args, const fs::path& dir) {
    std::vector<std::string> words;
    std::string date_arg;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "-d" || args[i] == "--date") {
            if (++i >= args.size()) throw std::runtime_error("missing value for --date");
            date_arg = args[i];
        } else {
            words.push_back(args[i]);
        }
    }
    if (words.size() < 2) throw std::runtime_error("usage: spent add AMOUNT CATEGORY [note...] [-d DATE]");

    Expense e;
    if (!parse_money(words[0], e.cents))
        throw std::runtime_error("'" + words[0] + "' is not a valid amount (use e.g. 85 or 1200.50)");
    e.category = normalize_category(words[1]);
    if (e.category.empty()) throw std::runtime_error("category cannot be empty");
    e.date = resolve_date(date_arg);
    if (e.date.empty()) throw std::runtime_error("invalid date '" + date_arg + "' (use YYYY-MM-DD, today or yesterday)");
    for (std::size_t i = 2; i < words.size(); ++i) e.note += (i > 2 ? " " : "") + words[i];

    fs::create_directories(dir);
    auto items = load_expenses(dir / "expenses.csv");
    e.id = next_id(items);
    items.push_back(e);
    save_expenses(dir / "expenses.csv", items);

    std::cout << "Added #" << e.id << ": " << money(e.cents) << " " << e.category << " (" << e.date << ")\n";

    // feedback: month total for this category and budget status
    std::string month = e.date.substr(0, 7);
    long long used = total(filter(items, month, e.category));
    std::cout << e.category << " in " << month_label(month) << ": " << money(used);
    auto budgets = load_budgets(dir / "budgets.csv");
    if (auto it = budgets.find(e.category); it != budgets.end()) {
        long long b = it->second;
        std::cout << " of " << money(b) << " (" << (used * 100 / b) << "%)";
        if (used > b) std::cout << "  ** OVER BUDGET by " << money(used - b) << " **";
    }
    std::cout << "\n";
    return 0;
}

static int cmd_list(const std::vector<std::string>& args, const fs::path& dir) {
    std::string month_arg, category;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if ((args[i] == "-m" || args[i] == "--month") && i + 1 < args.size()) month_arg = args[++i];
        else if ((args[i] == "-c" || args[i] == "--category") && i + 1 < args.size()) category = normalize_category(args[++i]);
        else throw std::runtime_error("unknown argument '" + args[i] + "'");
    }
    std::string month = resolve_month(month_arg);
    if (month.empty()) throw std::runtime_error("invalid month '" + month_arg + "' (use YYYY-MM, this, last or all)");

    auto rows = filter(load_expenses(dir / "expenses.csv"), month, category);
    if (rows.empty()) { std::cout << "No expenses found.\n"; return 0; }

    std::size_t w_amt = 0, w_cat = 0;
    for (const auto& e : rows) {
        w_amt = std::max(w_amt, money(e.cents).size());
        w_cat = std::max(w_cat, e.category.size());
    }
    for (const auto& e : rows)
        std::cout << std::right << std::setw(4) << ("#" + std::to_string(e.id)) << "  " << e.date << "  "
                  << std::setw(static_cast<int>(w_amt)) << money(e.cents) << "  " << std::left
                  << std::setw(static_cast<int>(w_cat)) << e.category << "  " << e.note << "\n";
    std::cout << "\n" << rows.size() << " expense(s), total " << money(total(rows)) << "\n";
    return 0;
}

static int cmd_summary(const std::vector<std::string>& args, const fs::path& dir) {
    std::string month_arg;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if ((args[i] == "-m" || args[i] == "--month") && i + 1 < args.size()) month_arg = args[++i];
        else throw std::runtime_error("unknown argument '" + args[i] + "'");
    }
    std::string month = resolve_month(month_arg);
    if (month.empty()) throw std::runtime_error("invalid month '" + month_arg + "' (use YYYY-MM, this, last or all)");

    auto rows = filter(load_expenses(dir / "expenses.csv"), month, "");
    std::cout << (month == "all" ? std::string("All time") : month_label(month)) << "\n";
    if (rows.empty()) { std::cout << "No expenses yet.\n"; return 0; }

    auto budgets = load_budgets(dir / "budgets.csv");
    auto cats = totals_by_category(rows);
    long long sum = total(rows);

    std::size_t w_cat = 0, w_amt = 0;
    for (const auto& c : cats) {
        w_cat = std::max(w_cat, c.category.size());
        w_amt = std::max(w_amt, money(c.cents).size());
    }
    w_amt = std::max(w_amt, money(sum).size());

    std::cout << repeat('-', static_cast<int>(w_cat + w_amt + 40)) << "\n";
    for (const auto& c : cats) {
        int pct = static_cast<int>((c.cents * 100 + sum / 2) / sum);
        int bar = static_cast<int>((c.cents * 20 + sum / 2) / sum);
        std::cout << std::left << std::setw(static_cast<int>(w_cat)) << c.category << "  " << std::right
                  << std::setw(static_cast<int>(w_amt)) << money(c.cents) << "  " << std::left << std::setw(20)
                  << repeat('#', bar) << " " << std::right << std::setw(3) << pct << "%";
        if (month != "all") {
            if (auto it = budgets.find(c.category); it != budgets.end()) {
                long long b = it->second;
                if (c.cents > b) std::cout << "   OVER budget by " << money(c.cents - b);
                else std::cout << "   " << (c.cents * 100 / b) << "% of " << money(b) << " budget";
            }
        }
        std::cout << "\n";
    }
    std::cout << repeat('-', static_cast<int>(w_cat + w_amt + 40)) << "\n";
    std::cout << std::left << std::setw(static_cast<int>(w_cat)) << "total" << "  " << std::right
              << std::setw(static_cast<int>(w_amt)) << money(sum) << "\n";

    if (month != "all") {
        std::string now = today_str(0);
        int y = std::stoi(month.substr(0, 4)), m = std::stoi(month.substr(5, 2));
        int days = (now.compare(0, 7, month) == 0) ? std::stoi(now.substr(8, 2)) : days_in_month(y, m);
        std::cout << "daily average: " << money(sum / days) << "  (" << rows.size() << " expenses over " << days
                  << " days)\n";
    }
    return 0;
}

static int cmd_remove(const std::vector<std::string>& args, const fs::path& dir) {
    if (args.size() != 1) throw std::runtime_error("usage: spent remove ID");
    int id = 0;
    try { id = std::stoi(args[0]); } catch (...) { throw std::runtime_error("'" + args[0] + "' is not a valid id"); }
    auto items = load_expenses(dir / "expenses.csv");
    auto it = std::find_if(items.begin(), items.end(), [&](const Expense& e) { return e.id == id; });
    if (it == items.end()) throw std::runtime_error("no expense with id " + std::to_string(id));
    std::cout << "Removed #" << it->id << ": " << money(it->cents) << " " << it->category << " " << it->note << "\n";
    items.erase(it);
    save_expenses(dir / "expenses.csv", items);
    return 0;
}

static int cmd_budget(const std::vector<std::string>& args, const fs::path& dir) {
    auto file = dir / "budgets.csv";
    auto budgets = load_budgets(file);
    if (args.empty()) {
        if (budgets.empty()) { std::cout << "No budgets set. Try: spent budget set food 3000\n"; return 0; }
        for (const auto& [c, v] : budgets) std::cout << std::left << std::setw(14) << c << money(v) << " / month\n";
        return 0;
    }
    if (args[0] == "set" && args.size() == 3) {
        long long cents;
        if (!parse_money(args[2], cents)) throw std::runtime_error("'" + args[2] + "' is not a valid amount");
        std::string cat = normalize_category(args[1]);
        fs::create_directories(dir);
        budgets[cat] = cents;
        save_budgets(file, budgets);
        std::cout << "Budget for " << cat << ": " << money(cents) << " / month\n";
        return 0;
    }
    if ((args[0] == "rm" || args[0] == "remove") && args.size() == 2) {
        std::string cat = normalize_category(args[1]);
        if (!budgets.erase(cat)) throw std::runtime_error("no budget for '" + cat + "'");
        save_budgets(file, budgets);
        std::cout << "Removed budget for " << cat << "\n";
        return 0;
    }
    throw std::runtime_error("usage: spent budget [set CATEGORY AMOUNT | rm CATEGORY]");
}

int main(int argc, char** argv) {
    try {
        if (argc < 2) { std::cout << kUsage; return 0; }
        std::string cmd = argv[1];
        std::vector<std::string> args(argv + 2, argv + argc);
        fs::path dir = data_dir();

        if (cmd == "add" || cmd == "a") return cmd_add(args, dir);
        if (cmd == "list" || cmd == "ls") return cmd_list(args, dir);
        if (cmd == "summary" || cmd == "sum") return cmd_summary(args, dir);
        if (cmd == "remove" || cmd == "rm") return cmd_remove(args, dir);
        if (cmd == "budget") return cmd_budget(args, dir);
        if (cmd == "where") { std::cout << dir.string() << "\n"; return 0; }
        if (cmd == "-h" || cmd == "--help" || cmd == "help") { std::cout << kUsage; return 0; }
        if (cmd == "-V" || cmd == "--version") { std::cout << "spent " << kVersion << "\n"; return 0; }
        throw std::runtime_error("unknown command '" + cmd + "' (try: spent --help)");
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
