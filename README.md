# spent

A tiny expense tracker for the command line. Log what you spend in one line, see where your money goes, and get warned when you go over budget.

- One-line logging: `spent add 85 food lunch`
- Monthly summary with per-category bars and daily average
- Optional monthly budgets with over-budget warnings
- Your data is plain **CSV** (opens in Excel / Google Sheets), stored on your own machine
- Money is stored as integers, so there are no floating-point rounding errors
- C++17, no dependencies, single small binary

## Build

```bash
g++ -std=c++17 -O2 -Iinclude src/main.cpp src/core.cpp -o spent
```

Or with CMake:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

## Usage

```bash
spent add 85 food lunch with team        # log an expense (date defaults to today)
spent add 1200.50 rent -d 2026-10-01     # specific date (also: -d yesterday)
spent budget set food 3000               # monthly budget for a category
spent summary                            # this month by category
spent list -m last -c food               # last month, food only
spent remove 7                           # delete expense #7
spent where                              # where the data lives
```

Example summary:

```
October 2026
---------------------------------------------------------
rent       1,200.50  #################     87%
food         125.00  ##                     9%   OVER budget by 25.00
transport     55.00  #                      4%
---------------------------------------------------------
total      1,380.50
daily average: 76.69  (4 expenses over 18 days)
```

Adding an expense shows where you stand right away:

```
Added #3: 40.00 food (2026-10-17)
food in October 2026: 125.00 of 100.00 (125%)  ** OVER BUDGET by 25.00 **
```

### Commands and shortcuts

| Command | Shortcut |
| --- | --- |
| `add` | `a` |
| `list` | `ls` |
| `summary` | `sum` |
| `remove` | `rm` |

`MONTH` can be `this`, `last`, `YYYY-MM` or `all`. `DATE` can be `today`, `yesterday` or `YYYY-MM-DD`.

### Settings

| Variable | Purpose |
| --- | --- |
| `SPENT_DIR` | Where data is stored (default `~/.spent`) |
| `SPENT_CURRENCY` | Text shown before amounts, e.g. `export SPENT_CURRENCY='$'` |

## Your data

Two plain files in `~/.spent/` (find them with `spent where`):

- `expenses.csv`: `id,date,cents,category,note`
- `budgets.csv`: `category,cents`

Back them up by copying the folder. Files are written atomically, so a crash can't leave a half-written file.

## License

MIT
