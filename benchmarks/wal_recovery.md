# WAL Recovery Benchmark

## Environment
- Machine: Mac Mini
- Storage: Local WAL-backed KV store (VeriStore)
- Workload: Cache metadata entries

## Results

| Entries | Write (ms) | Startup Recovery (ms) | Lookup (ms) | Total (ms) |
|--------:|-----------:|---------------------:|------------:|-----------:|
| 100     | 2          | 0                    | 0           | ~7         |
| 1,000   | 12         | 3                    | 2           | ~9         |
| 5,000   | 35         | 9                    | 8           | ~20        |

## Interpretation

- WAL replay overhead is minimal
- Recovery time grows slowly with dataset size
- Recovery cost is negligible compared to LLM inference

## Notes

- "Startup Recovery" = WAL open/replay time
- "Lookup" = metadata verification after recovery
- Total includes process startup overhead