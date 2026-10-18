#include "spent/core.hpp"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace spent {

// ---------------- money ----------------

bool parse_money(const std::string& text, long long& cents) {
    std::string s;
    for (char c : text)
        if (c != ',') s += c;  // allow 1,200.50
    if (s.empty()) return false;

    long long whole = 0, frac = 0;
    int frac_digits = 0, whole_digits = 0;
    bool seen_dot = false;
    for (char c : s) {
        if (c == '.') {
            if (seen_dot) return false;
            seen_dot = true;
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            if (!seen_dot) {
                if (++whole_digits > 12) return false;
                whole = whole * 10 + (c - '0');
            } else {
                if (++frac_digits > 2) return false;
                frac = frac * 10 + (c - '0');
            }
        } else {
            return false;
        }
    }
    if (whole_digits == 0 && frac_digits == 0) return false;
    if (frac_digits == 1) frac *= 10;
    cents = whole * 100 + frac;
    return cents > 0;
}

std::string format_money(long long cents) {
    bool neg = cents < 0;
    if (neg) cents = -cents;
    std::string whole = std::to_string(cents / 100);
    for (int i = static_cast<int>(whole.size()) - 3; i > 0; i -= 3) whole.insert(i, ",");
    std::string frac = std::to_string(cents % 100);
    if (frac.size() < 2) frac.insert(0, "0");
    return (neg ? "-" : "") + whole + "." + frac;
}

// ---------------- dates ----------------

int days_in_month(int y, int m) {
    static const int d[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
    return d[m - 1];
}

static bool all_digits(const std::string& s) {
    return !s.empty() &&
           std::all_of(s.begin(), s.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)); });
}

bool valid_date(const std::string& s) {
    if (s.size() != 10 || s[4] != '-' || s[7] != '-') return false;
    std::string ys = s.substr(0, 4), ms = s.substr(5, 2), ds = s.substr(8, 2);
    if (!all_digits(ys) || !all_digits(ms) || !all_digits(ds)) return false;
    int y = std::stoi(ys), m = std::stoi(ms), d = std::stoi(ds);
    return y >= 1900 && y <= 2200 && m >= 1 && m <= 12 && d >= 1 && d <= days_in_month(y, m);
}

std::string today_str(int day_offset) {
    std::time_t t = std::time(nullptr) + static_cast<std::time_t>(day_offset) * 86400;
    std::tm* tm = std::localtime(&t);
    char buf[16];
    std::strftime(buf, sizeof buf, "%Y-%m-%d", tm);
    return buf;
}

std::string resolve_date(const std::string& in) {
    if (in.empty() || in == "today") return today_str(0);
    if (in == "yesterday") return today_str(-1);
    return valid_date(in) ? in : "";
}

std::string resolve_month(const std::string& in) {
    if (in.empty() || in == "this") return today_str(0).substr(0, 7);
    if (in == "all") return "all";
    if (in == "last") {
        std::string t = today_str(0);
        int y = std::stoi(t.substr(0, 4)), m = std::stoi(t.substr(5, 2));
        if (--m == 0) { m = 12; --y; }
        char buf[32];
        std::snprintf(buf, sizeof buf, "%04d-%02d", y, m);
        return buf;
    }
    if (in.size() == 7 && in[4] == '-' && all_digits(in.substr(0, 4)) && all_digits(in.substr(5, 2))) {
        int m = std::stoi(in.substr(5, 2));
        if (m >= 1 && m <= 12) return in;
    }
    return "";
}

std::string month_label(const std::string& ym) {
    static const char* names[] = {"January", "February", "March",     "April",   "May",      "June",
                                  "July",    "August",   "September", "October", "November", "December"};
    int m = std::stoi(ym.substr(5, 2));
    return std::string(names[m - 1]) + " " + ym.substr(0, 4);
}

// ---------------- text / csv ----------------

std::string normalize_category(std::string s) {
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string csv_escape(const std::string& f) {
    if (f.find_first_of(",\"\n\r") == std::string::npos) return f;
    std::string out = "\"";
    for (char c : f) {
        if (c == '"') out += '"';
        out += c;
    }
    return out + "\"";
}

std::vector<std::string> csv_split(const std::string& line) {
    std::vector<std::string> fields;
    std::string cur;
    bool quoted = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (quoted) {
            if (c == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') { cur += '"'; ++i; }
                else quoted = false;
            } else {
                cur += c;
            }
        } else if (c == '"') {
            quoted = true;
        } else if (c == ',') {
            fields.push_back(cur);
            cur.clear();
        } else if (c != '\r') {
            cur += c;
        }
    }
    fields.push_back(cur);
    return fields;
}

// ---------------- storage ----------------

static void write_atomically(const fs::path& file, const std::string& content) {
    fs::path tmp = file;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) throw std::runtime_error("cannot write " + tmp.string());
        out << content;
        if (!out) throw std::runtime_error("write failed for " + tmp.string());
    }
    fs::rename(tmp, file);  // never leaves a half-written data file
}

std::vector<Expense> load_expenses(const fs::path& file) {
    std::vector<Expense> items;
    std::ifstream in(file);
    if (!in) return items;  // first run: no file yet
    std::string line;
    while (std::getline(in, line)) {
        auto f = csv_split(line);
        if (f.size() < 4 || !all_digits(f[0]) || !all_digits(f[2]) || !valid_date(f[1])) continue;  // header/bad row
        Expense e;
        e.id = std::stoi(f[0]);
        e.date = f[1];
        e.cents = std::stoll(f[2]);
        e.category = f[3];
        e.note = f.size() > 4 ? f[4] : "";
        items.push_back(e);
    }
    return items;
}

void save_expenses(const fs::path& file, const std::vector<Expense>& items) {
    std::ostringstream out;
    out << "id,date,cents,category,note\n";
    for (const auto& e : items)
        out << e.id << ',' << e.date << ',' << e.cents << ',' << csv_escape(e.category) << ','
            << csv_escape(e.note) << '\n';
    write_atomically(file, out.str());
}

std::map<std::string, long long> load_budgets(const fs::path& file) {
    std::map<std::string, long long> b;
    std::ifstream in(file);
    std::string line;
    while (std::getline(in, line)) {
        auto f = csv_split(line);
        if (f.size() >= 2 && all_digits(f[1])) b[f[0]] = std::stoll(f[1]);
    }
    return b;
}

void save_budgets(const fs::path& file, const std::map<std::string, long long>& budgets) {
    std::ostringstream out;
    out << "category,cents\n";
    for (const auto& [cat, cents] : budgets) out << csv_escape(cat) << ',' << cents << '\n';
    write_atomically(file, out.str());
}

// ---------------- queries ----------------

std::vector<Expense> filter(const std::vector<Expense>& items, const std::string& month,
                            const std::string& category) {
    std::vector<Expense> out;
    for (const auto& e : items) {
        if (month != "all" && e.date.compare(0, 7, month) != 0) continue;
        if (!category.empty() && e.category != category) continue;
        out.push_back(e);
    }
    std::sort(out.begin(), out.end(), [](const Expense& a, const Expense& b) {
        return a.date != b.date ? a.date < b.date : a.id < b.id;
    });
    return out;
}

std::vector<CategoryTotal> totals_by_category(const std::vector<Expense>& items) {
    std::map<std::string, long long> m;
    for (const auto& e : items) m[e.category] += e.cents;
    std::vector<CategoryTotal> out;
    for (const auto& [c, v] : m) out.push_back({c, v});
    std::sort(out.begin(), out.end(), [](const CategoryTotal& a, const CategoryTotal& b) {
        return a.cents != b.cents ? a.cents > b.cents : a.category < b.category;
    });
    return out;
}

long long total(const std::vector<Expense>& items) {
    long long t = 0;
    for (const auto& e : items) t += e.cents;
    return t;
}

}  // namespace spent
