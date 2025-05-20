const std = @import("std");
const fs = std.fs;
const mem = std.mem;
const path = std.fs.path;

const buildExe = @import("build/exe.zig");
const compile_command = @import("build/compile_command.zig");

const flags = [_][]const u8{
    "-std=c++23",
    "-fexperimental-library",
    "-mavx2",
};

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const exe = buildExe.set(b, target, optimize, &flags);

    compile_command.generate(b, &[_]*std.Build.Step.Compile{exe});
}
