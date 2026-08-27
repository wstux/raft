window.BENCHMARK_DATA = {
  "lastUpdate": 1787840590367,
  "repoUrl": "https://github.com/wstux/raft",
  "entries": {
    "Benchmark": [
      {
        "commit": {
          "author": {
            "email": "wstux1@gmail.com",
            "name": "wstux",
            "username": "wstux"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "4273ccde0cd9ee2869a5d8c833f372c6d7dbc0df",
          "message": "* all: #1 Merge remote-tracking branch 'wstux/feature/rename';",
          "timestamp": "2026-08-27T17:20:13+03:00",
          "tree_id": "9b58949f01e357d58a34c0a3aa6f5d1bf8508619",
          "url": "https://github.com/wstux/raft/commit/4273ccde0cd9ee2869a5d8c833f372c6d7dbc0df"
        },
        "date": 1787840589330,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "Leader Election (Real Time)::3 nodes",
            "value": 41.75357233,
            "unit": "ms"
          },
          {
            "name": "Leader Election (CPU Time)::3 nodes",
            "value": 0.4601678800000005,
            "unit": "ms"
          },
          {
            "name": "Leader Election (Real Time)::5 nodes",
            "value": 42.13183481000215,
            "unit": "ms"
          },
          {
            "name": "Leader Election (CPU Time)::5 nodes",
            "value": 0.9479616899999956,
            "unit": "ms"
          },
          {
            "name": "Leader Election (Real Time)::7 nodes",
            "value": 44.617223290001675,
            "unit": "ms"
          },
          {
            "name": "Leader Election (CPU Time)::7 nodes",
            "value": 1.5507828199999865,
            "unit": "ms"
          },
          {
            "name": "Leader Election Default (Real Time)::3 nodes",
            "value": 585.2363073000078,
            "unit": "ms"
          },
          {
            "name": "Leader Election Default (CPU Time)::3 nodes",
            "value": 1.0433858999999046,
            "unit": "ms"
          },
          {
            "name": "Leader Election Default (Real Time)::5 nodes",
            "value": 599.9535201999862,
            "unit": "ms"
          },
          {
            "name": "Leader Election Default (CPU Time)::5 nodes",
            "value": 1.627771800000022,
            "unit": "ms"
          },
          {
            "name": "Leader Election Default (Real Time)::7 nodes",
            "value": 565.6756616999985,
            "unit": "ms"
          },
          {
            "name": "Leader Election Default (CPU Time)::7 nodes",
            "value": 2.0323026000000244,
            "unit": "ms"
          },
          {
            "name": "Check Contact Quorum (Real Time)::1 threads",
            "value": 5.772486494135663,
            "unit": "ns"
          },
          {
            "name": "Check Contact Quorum (Real Time)::2 threads",
            "value": 31.076267719167237,
            "unit": "ns"
          },
          {
            "name": "Check Contact Quorum (Real Time)::4 threads",
            "value": 118.5014836886255,
            "unit": "ns"
          },
          {
            "name": "Check Contact Quorum (Real Time)::8 threads",
            "value": 183.27518732152816,
            "unit": "ns"
          },
          {
            "name": "Request to List (Real Time)::1 threads",
            "value": 22.925595596218436,
            "unit": "ns"
          },
          {
            "name": "Request to List (Real Time)::2 threads",
            "value": 23.10120791398189,
            "unit": "ns"
          },
          {
            "name": "Request to List (Real Time)::4 threads",
            "value": 46.214102603468376,
            "unit": "ns"
          },
          {
            "name": "Request to List (Real Time)::8 threads",
            "value": 97.12203529858714,
            "unit": "ns"
          },
          {
            "name": "Request Lock List (Real Time)::1 threads",
            "value": 1.5565183487163148,
            "unit": "ns"
          },
          {
            "name": "Request Lock List (Real Time)::2 threads",
            "value": 1.560319697960915,
            "unit": "ns"
          },
          {
            "name": "Request Lock List (Real Time)::4 threads",
            "value": 3.726196165364561,
            "unit": "ns"
          },
          {
            "name": "Request Lock List (Real Time)::8 threads",
            "value": 5.8340824490613725,
            "unit": "ns"
          },
          {
            "name": "Serialize Message (Real Time)::Base",
            "value": 0.6269904617047052,
            "unit": "ns"
          }
        ]
      }
    ]
  }
}