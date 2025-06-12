const std = @import("std");

const cppfiles = [_][]const u8{
    "src/main.cpp",

    "src/api/cli.cpp",
    "src/api/lib.cpp",

    "src/common/md5.cpp",
    "src/common/utils.cpp",

    "src/file/file.cpp",
    "src/file/sac.cpp",
    "src/file/wav.cpp",

    "src/libsac/libsac.cpp",
    "src/libsac/map.cpp",
    "src/libsac/pred.cpp",
    "src/libsac/profile.cpp",
    "src/libsac/vle.cpp",

    "src/model/range.cpp",

    "src/opt/cma.cpp",
    "src/opt/dds.cpp",
    "src/opt/de.cpp",
    "src/opt/opt.cpp",

    "src/pred/rls.cpp",
};

pub fn set(b: *std.Build, target: std.Build.ResolvedTarget, optimize: std.builtin.OptimizeMode, flags: []const []const u8) *std.Build.Step.Compile {
    const exe = b.addExecutable(.{
        .name = "sac",
        .target = target,
        .optimize = optimize,
    });

    exe.addCSourceFiles(.{
        .files = &cppfiles,
        .flags = flags,
    });

    if (target.result.abi != .msvc) {
        exe.linkLibCpp();
    } else {
        exe.linkLibC();
    }

    b.installArtifact(exe);

    return exe;
}
