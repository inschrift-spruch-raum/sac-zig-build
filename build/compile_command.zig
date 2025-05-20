const zcc = @import("compile_commands");
const std = @import("std");

pub fn generate(b: *std.Build, buildSets: []const *std.Build.Step.Compile) void {
    var targets = std.ArrayList(*std.Build.Step.Compile).init(b.allocator);
    for (buildSets) |buildSet| {
        targets.append(buildSet) catch @panic("OOM");
    }
    zcc.createStep(b, "cdb", targets.toOwnedSlice() catch @panic("OOM"));
}
