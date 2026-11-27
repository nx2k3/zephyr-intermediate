# Without Mutex
- Final expected value: 2000000
- Final value got: 1726051
- Execution time: 146 ms
- Race condition confirmed: lost 273949 updates

# With Mutex
- Final expected value: 2000000
- Final value got: 2000000
- Execution time: 38460 ms
- No Race condition

# Observation
- using mutex solves the race condition but the execution time is more
- for lower values like 100,1000,10000 there was no race condition
