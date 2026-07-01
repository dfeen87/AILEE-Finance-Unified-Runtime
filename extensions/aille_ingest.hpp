#ifndef AILLE_INGEST_HPP
#define AILLE_INGEST_HPP

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <stdexcept>
#include "aille_hal.hpp" // Needs to know about NICHostRuntime

namespace AILLE {
namespace Ingest {

// ============================================================================
// ZERO-COPY INGEST PIPELINE STRUCTURES
// ============================================================================

// DMA-friendly RX Descriptor
// Represents an entry in a ring buffer populated directly by hardware (e.g. NIC)
struct alignas(64) DMARXDescriptor {
    uint64_t physical_addr;    // Pointer to packet payload
    uint32_t length;           // Length of the packet
    uint16_t status_flags;     // Checksum OK, errors, etc.
    uint16_t metadata;         // Receive queue ID, timestamping info, etc.
    uint64_t hardware_timestamp; // Hardware-stamped time of arrival
};

// PacketView
// Zero-copy wrapper around raw memory. Eliminates JSON translation or heap allocation.
class PacketView {
private:
    const uint8_t* data_;
    size_t length_;
    const DMARXDescriptor* descriptor_;

public:
    PacketView(const uint8_t* data, size_t length, const DMARXDescriptor* descriptor = nullptr)
        : data_(data), length_(length), descriptor_(descriptor) {}

    [[nodiscard]] const uint8_t* data() const noexcept { return data_; }
    [[nodiscard]] size_t length() const noexcept { return length_; }
    [[nodiscard]] const DMARXDescriptor* descriptor() const noexcept { return descriptor_; }

    [[nodiscard]] std::string_view asStringView() const noexcept {
        return std::string_view(reinterpret_cast<const char*>(data_), length_);
    }

    [[nodiscard]] bool empty() const noexcept { return length_ == 0; }
};

// ============================================================================
// PROTOCOL PARSERS
// ============================================================================

// Base structure for a parsed normalized event
struct NormalizedEvent {
    uint64_t symbol_id;
    float price;
    float quantity;
    int side; // 1 for Buy, -1 for Sell
    uint64_t timestamp_ns;
    bool is_valid;

    NormalizedEvent() : symbol_id(0), price(0.0f), quantity(0.0f), side(0), timestamp_ns(0), is_valid(false) {}
};

class FIXParser {
public:
    static NormalizedEvent parse(const PacketView& view) {
        NormalizedEvent event;
        event.is_valid = false;

        if (view.empty()) return event;

        std::string_view sv = view.asStringView();
        size_t pos = 0;

        // Very basic tag-value parsing simulating a FIX stream
        while (pos < sv.length()) {
            size_t eq_pos = sv.find('=', pos);
            if (eq_pos == std::string_view::npos) break;

            size_t delim_pos = sv.find('\x01', eq_pos); // SOH character
            if (delim_pos == std::string_view::npos) break; // Or could use ';' for testing

            std::string_view tag_str = sv.substr(pos, eq_pos - pos);
            std::string_view val_str = sv.substr(eq_pos + 1, delim_pos - eq_pos - 1);

            try {
                int tag = std::stoi(std::string(tag_str));

                switch (tag) {
                    case 48: // SecurityID
                        event.symbol_id = std::stoull(std::string(val_str));
                        break;
                    case 44: // Price
                        event.price = std::stof(std::string(val_str));
                        break;
                    case 38: // OrderQty
                        event.quantity = std::stof(std::string(val_str));
                        break;
                    case 54: // Side (1=Buy, 2=Sell)
                        {
                            int raw_side = std::stoi(std::string(val_str));
                            event.side = (raw_side == 1) ? 1 : -1;
                        }
                        break;
                }
            } catch (...) {
                // Ignore parse errors for simulated simplicity
            }

            pos = delim_pos + 1;
        }

        if (event.price > 0 && event.quantity > 0) {
            event.is_valid = true;
        }

        return event;
    }
};

class FASTDecoder {
public:
    static NormalizedEvent decode(const PacketView& view) {
        NormalizedEvent event;
        event.is_valid = false;

        if (view.empty()) return event;

        const uint8_t* data = view.data();
        size_t len = view.length();
        size_t pos = 0;

        // Extremely simplified simulation of FAST decoding (stop bit encoding)
        auto read_uint = [&]() -> uint64_t {
            uint64_t val = 0;
            while (pos < len) {
                uint8_t b = data[pos++];
                val = (val << 7) | (b & 0x7F);
                if (b & 0x80) break; // Stop bit
            }
            return val;
        };

        if (pos < len) event.symbol_id = read_uint();
        if (pos < len) event.price = static_cast<float>(read_uint()) / 100.0f; // Mock scaling
        if (pos < len) event.quantity = static_cast<float>(read_uint());
        if (pos < len) event.side = (read_uint() == 1) ? 1 : -1;

        if (event.price > 0 && event.quantity > 0) {
            event.is_valid = true;
        }

        return event;
    }
};

class ITCHParser {
public:
    static NormalizedEvent parse(const PacketView& view) {
        NormalizedEvent event;
        event.is_valid = false;

        if (view.length() < 1) return event;

        const uint8_t* data = view.data();
        char msg_type = static_cast<char>(data[0]);

        // Very basic simulation of ITCH Add Order Message (Type 'A' or 'F')
        if (msg_type == 'A' || msg_type == 'F') {
            // Simplified fixed offsets for mock parsing
            if (view.length() >= 36) { // Minimum length for A/F message in our simplified mock
                // In reality, we'd use proper endian swapping (e.g. be32toh)
                // We'll mock it by just casting for this simulation

                // Assuming simple struct layout for mock:
                // 0: MsgType (1)
                // 1: StockLocate (2)
                // 3: TrackingNum (2)
                // 5: Timestamp (6)
                // 11: OrderRefNum (8)
                // 19: Buy/Sell Indicator (1)
                // 20: Shares (4)
                // 24: Stock (8)
                // 32: Price (4)

                event.side = (data[19] == 'B') ? 1 : -1;
                event.quantity = static_cast<float>(*reinterpret_cast<const uint32_t*>(data + 20));

                // For simplicity, just hash the stock string to get an ID
                std::string_view stock(reinterpret_cast<const char*>(data + 24), 8);
                event.symbol_id = std::hash<std::string_view>{}(stock);

                event.price = static_cast<float>(*reinterpret_cast<const uint32_t*>(data + 32)) / 10000.0f; // Mock 4-implied decimal

                if (event.price > 0 && event.quantity > 0) {
                    event.is_valid = true;
                }
            }
        }

        return event;
    }
};

class CMEMDP3Parser {
public:
    static NormalizedEvent parse(const PacketView& view) {
        NormalizedEvent event;
        event.is_valid = false;

        if (view.length() < 8) return event; // Minimum length

        // SBE (Simple Binary Encoding) mock parsing
        // In reality, this requires a full SBE decoder
        const uint8_t* data = view.data();

        // Mocking a simplified SBE layout
        // 0: Message Size (2)
        // 2: Block Length (2)
        // 4: Template ID (2)
        // 6: Schema ID (2)
        // 8: Version (2)
        // 10: Payload...

        uint16_t template_id = *reinterpret_cast<const uint16_t*>(data + 4);

        // E.g., MDIncrementalRefresh (Template 32)
        if (template_id == 32 && view.length() >= 32) {
            // Mocking extracting first repeating group entry
            // Let's assume payload starts at 10 and we have one MDEntry
            // 10: MDUpdateAction (1)
            // 11: MDEntryType (1) // '0'=Bid, '1'=Offer
            // 12: SecurityID (4)
            // 16: MDEntryPx (8) // usually a mantissa/exponent but we'll mock as double
            // 24: MDEntrySize (4)

            char entry_type = static_cast<char>(data[11]);
            event.side = (entry_type == '0') ? 1 : -1;

            event.symbol_id = *reinterpret_cast<const uint32_t*>(data + 12);

            // Mock price parsing (in reality, CME uses custom decimal struct)
            event.price = static_cast<float>(*reinterpret_cast<const double*>(data + 16));
            event.quantity = static_cast<float>(*reinterpret_cast<const uint32_t*>(data + 24));

            if (event.price > 0 && event.quantity > 0) {
                event.is_valid = true;
            }
        }

        return event;
    }
};

// ============================================================================
// INLINE RISK EVALUATION
// ============================================================================

template <typename Parser>
class InlineRiskFeedParser {
private:
    AILLE::HAL::NICHostRuntime& runtime_;

public:
    explicit InlineRiskFeedParser(AILLE::HAL::NICHostRuntime& runtime) : runtime_(runtime) {}

    // Directly emit into consensus/safety path before userland queuing
    Decision processPacket(const PacketView& view) {
        NormalizedEvent event = Parser::parse(view);

        if (!event.is_valid) {
            Decision d;
            d.status = ERROR_NO_MODELS;
            d.setReasoning("Invalid packet or failed to parse event");
            return d;
        }

        // Convert the normalized market data event into a ModelSignal.
        // In a real system, the price/qty might be evaluated against a risk model first.
        // For this inline risk feed, we'll map side * quantity to a mock value,
        // and assign a high confidence to represent direct market data integrity.
        ModelSignal signal;
        signal.value = static_cast<float>(event.side) * event.quantity;
        signal.confidence = 0.95f; // High confidence for raw market data

        if (view.descriptor() != nullptr) {
            signal.timestamp_ns = view.descriptor()->hardware_timestamp;
        } else {
            // Fallback if no HW timestamp
            signal.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()
            ).count();
        }

        signal.model_id = static_cast<int>(event.symbol_id % 1000); // Mock mapping

        std::vector<ModelSignal> signals;
        signals.push_back(signal);

        // Emit directly into the hardware/consensus runtime
        return runtime_.processSignals(signals);
    }
};

} // namespace Ingest
} // namespace AILLE

#endif // AILLE_INGEST_HPP
