#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace spent {
namespace fs = std::filesystem;

struct Expense {
    int id = 0;
    std::string date;       // YYYY-MM-DD
    long long cents = 0;    // money is stored as integer hundredths: no float rounding errors
    std::string category;   // lowercase
    std::string note;
};

struct CategoryTotal {
    std::string category;
    long long cents = 0;
};

// ---- money ----
bool parse_money(const std::string& text, long long& cents);  // "1,200.5" -> 120050
std::string format_money(long long cents);                    // 120050 -> "1,200.50"

// ---- dates ----
int days_in_month(int year, int month);
bool valid_date(const std::string& ymd);
std::string today_str(int day_offset = 0);
std::string resolve_date(const std::string& input);   // ""/today/yesterday/YYYY-MM-DD, else ""
std::string resolve_month(const std::string& input);  // ""/this/last/YYYY-MM/all, else ""
std::string month_label(const std::string& ym);       // "2026-10" -> "October 2026"

// ---- text / csv ----
std::string normalize_category(std::string s);
std::string csv_escape(const std::string& field);
std::vector<std::string> csv_split(const std::string& line);

// ---- storage ----
std::vector<Expense> load_expenses(const fs::path& file);
void save_expenses(const fs::path& file, const std::vector<Expense>& items);
std::map<std::string, long long> load_budgets(const fs::path& file);
void save_budgets(const fs::path& file, const std::map<std::string, long long>& budgets);

// ---- queries ----
// month: "YYYY-MM" or "all"; category: "" for any
std::vector<Expense> filter(const std::vector<Expense>& items, const std::string& month,
                            const std::string& category);
std::vector<CategoryTotal> totals_by_category(const std::vector<Expense>& items);
long long total(const std::vector<Expense>& items);

}  // namespace spent
