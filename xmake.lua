add_rules("mode.debug", "mode.release")

target("calloc")
    set_kind("static")
    add_files("src/*.c")
    set_languages("c17")

target("tests")
    set_kind("binary")
    add_files("tests/*.c")
    add_includedirs("src")
    add_deps("calloc")
    set_languages("c17")

task("format")
    on_run(function ()
        import("lib.detect.find_tool")

        local tool = find_tool("clang-format")
        if not tool then
            raise("clang-format not found")
        end

        for _, file in ipairs(os.files("src/**.c")) do
            os.execv(tool.program, {"-i", file})
        end

        for _, file in ipairs(os.files("src/**.h")) do
            os.execv(tool.program, {"-i", file})
        end
    end)

    set_menu {
        usage = "xmake format",
        description = "Format C source files"
    }

task("test")
    on_run(function ()
        import("core.project.task")
        import("core.project.project")

        local ok, errors = task.run("build", {"tests"})
        if ok == false then
            raise(errors or "failed to build the tests target")
        end

        local program = project.target("tests"):targetfile()

        if not os.isfile(program) then
            raise("tests binary not found: %s", program)
        end

        local code = os.execv(program)
        if code ~= 0 then
            raise("tests failed with exit code %d", code)
        end
    end)

    set_menu {
        usage = "xmake test",
        description = "Build and run the test suite"
    }

task("bench")
    on_run(function ()
        import("core.project.task")
        import("core.project.project")

        local ok, errors = task.run("build", {"bench"})
        if ok == false then
            raise(errors or "failed to build the bench target")
        end

        local program = project.target("bench"):targetfile()

        if not os.isfile(program) then
            raise("bench binary not found: %s", program)
        end

        local code = os.execv(program)
        if code ~= 0 then
            raise("bench failed with exit code %d", code)
        end
    end)

    set_menu {
        usage = "xmake bench",
        description = "Build and run the allocator benchmarks"
    }
