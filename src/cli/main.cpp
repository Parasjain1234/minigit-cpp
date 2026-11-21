#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "core/error.h"
#include "objects/oid.h"
#include "repo/repository.h"

namespace {

namespace fs = std::filesystem;
using minigit::MinigitError;
using minigit::StatusReport;

void printUsage(std::ostream& out) {
  out << "usage: minigit <command> [<args>]\n\n"
         "Commands:\n"
         "   init                      Create an empty MiniGit repository\n"
         "   add <path>...             Add file contents to the index\n"
         "   commit -m <message>       Record staged changes\n"
         "   status                    Show the working tree status\n\n"
         "Run 'minigit help' to see this message.\n";
}

void printStaged(const std::vector<minigit::StagedChange>& changes) {
  for (const auto& change : changes) {
    std::cout << '\t' << minigit::changeKindLabel(change.kind) << change.path
              << '\n';
  }
}

int commandInit(const std::vector<std::string>& args) {
  if (args.size() > 1) {
    throw MinigitError("init takes no arguments");
  }
  const fs::path directory = fs::current_path();
  const bool existed = fs::exists(directory / ".minigit");
  minigit::Repository::init(directory);
  std::cout << (existed ? "Reinitialized" : "Initialized")
            << " empty MiniGit repository in " << (directory / ".minigit").string()
            << "\n";
  return 0;
}

int commandAdd(const std::vector<std::string>& args) {
  if (args.size() < 2) {
    throw MinigitError("add requires at least one path");
  }

  minigit::Repository repo = minigit::Repository::open();
  std::vector<fs::path> paths;
  paths.reserve(args.size() - 1);
  for (std::size_t i = 1; i < args.size(); ++i) {
    paths.emplace_back(args[i]);
  }

  for (const std::string& path : repo.add(paths)) {
    std::cout << "add '" << path << "'\n";
  }
  return 0;
}

int commandCommit(const std::vector<std::string>& args) {
  std::string message;
  for (std::size_t i = 1; i < args.size(); ++i) {
    const std::string& arg = args[i];
    if (arg == "-m" || arg == "--message") {
      if (i + 1 >= args.size()) {
        throw MinigitError("commit: option '" + arg +
                           "' requires a message argument");
      }
      if (!message.empty()) {
        throw MinigitError("commit: multiple -m options are not supported");
      }
      message = args[++i];
    } else {
      throw MinigitError("commit: unknown argument '" + arg + "'");
    }
  }
  if (message.empty()) {
    throw MinigitError("commit requires a message: minigit commit -m \"...\"");
  }

  minigit::Repository repo = minigit::Repository::open();
  const minigit::Oid commitId = repo.commit(message);

  const std::string branch = repo.headBranch();
  std::cout << '[' << (branch.empty() ? "detached HEAD" : branch) << ' '
            << commitId.shortHex() << "] " << message << '\n';
  return 0;
}

int commandStatus(const std::vector<std::string>& args) {
  if (args.size() > 1) {
    throw MinigitError("status takes no arguments");
  }

  const minigit::Repository repo = minigit::Repository::open();
  const StatusReport report = repo.status();

  if (report.detached) {
    std::cout << "HEAD detached at " << report.branch << "\n\n";
  } else {
    std::cout << "On branch " << report.branch << "\n\n";
  }

  if (!report.hasCommits) {
    std::cout << "No commits yet\n\n";
  }

  bool printedSomething = false;

  if (!report.staged.empty()) {
    printedSomething = true;
    std::cout << "Changes to be committed:\n";
    printStaged(report.staged);
    std::cout << '\n';
  }

  if (!report.unstaged.empty()) {
    printedSomething = true;
    std::cout << "Changes not staged for commit:\n";
    printStaged(report.unstaged);
    std::cout << '\n';
  }

  if (!report.untracked.empty()) {
    printedSomething = true;
    std::cout << "Untracked files:\n";
    for (const std::string& path : report.untracked) {
      std::cout << '\t' << path << '\n';
    }
    std::cout << '\n';
  }

  if (!printedSomething) {
    std::cout << "nothing to commit, working tree clean\n";
  }
  return 0;
}

int run(const std::vector<std::string>& args) {
  if (args.empty()) {
    printUsage(std::cout);
    return 0;
  }

  const std::string& command = args.front();
  if (command == "help" || command == "--help" || command == "-h") {
    printUsage(std::cout);
    return 0;
  }
  if (command == "init") {
    return commandInit(args);
  }
  if (command == "add") {
    return commandAdd(args);
  }
  if (command == "commit") {
    return commandCommit(args);
  }
  if (command == "status") {
    return commandStatus(args);
  }

  throw MinigitError("unknown command '" + command +
                     "' (run 'minigit help' for usage)");
}

}  // namespace

int main(int argc, char** argv) {
  std::vector<std::string> args;
  args.reserve(static_cast<std::size_t>(argc));
  for (int i = 1; i < argc; ++i) {
    args.emplace_back(argv[i]);
  }

  try {
    return run(args);
  } catch (const MinigitError& error) {
    std::cerr << "minigit: error: " << error.what() << '\n';
    return 1;
  } catch (const std::exception& error) {
    std::cerr << "minigit: unexpected error: " << error.what() << '\n';
    return 1;
  }
}
