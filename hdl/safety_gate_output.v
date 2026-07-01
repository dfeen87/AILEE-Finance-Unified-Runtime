// Safety Gate Output
// Cycle budget: 5 cycles
// MUST remain strictly advisory (no execution capability)

module safety_gate_output (
    input wire clk,
    input wire rst,
    input wire [31:0] in_consensus_value,
    input wire [31:0] in_consensus_confidence,
    input wire in_valid,
    input wire safety_veto,

    output reg [31:0] final_value,
    output reg [31:0] final_confidence,
    output reg final_valid,
    output wire execution_capability
);

    // Explicitly tied to 0 to satisfy "advisory only" constraint
    assign execution_capability = 1'b0;

    // Skeleton implementation
    always @(posedge clk) begin
        if (rst) begin
            final_valid <= 1'b0;
            final_value <= 32'b0;
            final_confidence <= 32'b0;
        end else begin
            if (in_valid) begin
                if (safety_veto) begin
                    // Fallback to safe zero-position
                    final_value <= 32'b0;
                    final_confidence <= 32'b0;
                    final_valid <= 1'b1;
                end else begin
                    final_value <= in_consensus_value;
                    final_confidence <= in_consensus_confidence;
                    final_valid <= 1'b1;
                end
            end else begin
                final_valid <= 1'b0;
            end
        end
    end

endmodule
