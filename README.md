# Raft library

A C++ library implementing the Raft consensus algorithm. The architecture and
internal state logic are based on the [Canonical Raft](https://github.com/canonical/raft) repository.

The library is designed as a pure State Machine. It is isolated from specific
network protocols and storage systems, providing developers with interfaces for
custom integration.

## License

&copy; 2026 Chistyakov Alexander.

Open sourced under MIT license, the terms of which can be read here — [MIT License](http://opensource.org/licenses/MIT).
