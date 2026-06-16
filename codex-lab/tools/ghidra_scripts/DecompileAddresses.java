// Ghidra headless helper for dumping disassembly and decompiler output.
// Usage args: <output-file> <address> [address...]

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.listing.Listing;

import java.io.File;
import java.io.PrintWriter;

public class DecompileAddresses extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            println("Usage: DecompileAddresses <output-file> <address> [address...]");
            return;
        }

        File output = new File(args[0]);
        output.getParentFile().mkdirs();

        Listing listing = currentProgram.getListing();
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        try (PrintWriter out = new PrintWriter(output, "UTF-8")) {
            out.printf("Program: %s%n", currentProgram.getName());
            out.printf("ImageBase: %s%n%n", currentProgram.getImageBase());

            for (int i = 1; i < args.length; i++) {
                Address address = toAddr(args[i]);
                Function function = listing.getFunctionContaining(address);
                if (function == null) {
                    function = createFunction(address, "fn_" + address.toString());
                }

                out.printf("===== %s =====%n", args[i]);
                if (function == null) {
                    out.println("No function could be created or found.");
                    out.println();
                    continue;
                }

                out.printf("Function: %s%n", function.getName());
                out.printf("Entry: %s%n", function.getEntryPoint());
                out.printf("Body: %s%n%n", function.getBody());

                out.println("-- Disassembly --");
                InstructionIterator instructions = listing.getInstructions(function.getBody(), true);
                int count = 0;
                while (instructions.hasNext() && count < 500) {
                    Instruction instruction = instructions.next();
                    out.printf("%s: %s%n", instruction.getAddress(), instruction);
                    count++;
                }
                if (count >= 500) {
                    out.println("... truncated after 500 instructions");
                }

                out.println();
                out.println("-- Decompiler --");
                DecompileResults results = decompiler.decompileFunction(function, 60, monitor);
                if (results != null && results.decompileCompleted() && results.getDecompiledFunction() != null) {
                    out.println(results.getDecompiledFunction().getC());
                } else {
                    out.printf("Decompiler failed: %s%n", results == null ? "null result" : results.getErrorMessage());
                }
                out.println();
            }
        } finally {
            decompiler.dispose();
        }

        println("Wrote " + output.getAbsolutePath());
    }
}
