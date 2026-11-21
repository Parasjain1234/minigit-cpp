#include <cstdint>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "cli/graph.h"
#include "core/error.h"
#include "objects/oid.h"
#include "repo/history.h"
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
         "   status                    Show the working tree status\n"
         "   log [--graph]             Show commit history as a list or DAG\n"
         "   branch [<name>]           List branches or create one at HEAD\n"
         "   checkout <branch|commit>  Switch branches or restore a commit\n"
         "   diff [--staged] [--myers] Show line changes (LCS or Myers)\n"
         "   merge <branch> [-m <msg>] Merge a branch into the current one\n\n"
         "Run 'minigit help' to see this message.\n";
}

void printStaged(const std::vector<minigit::StagedChange>& changes) {
  for (const auto& change : changes) {
    std::cout << '\t' << minigit::changeKindLabel(change.kind) << change.path
              << '\n';
  }
}

std::string formatTimestamp(std::int64_t timestamp) {
  const std::time_t time = static_cast<std::time_t>(timestamp);
  std::tm broken{};
#ifdef _WIN32
  gmtime_s(&broken, &time);
#else
  gmtime_r(&time, &broken);
#endif
  char buffer[64] = {};
  std::strftime(buffer, sizeof(buffer), "%a %b %d %H:%M:%S %Y +0000",
                &broken);
  return buffer;
}

int commandInit(const std::vector<std::string>& args) {
  if (args.size() > 1) {
    throw MinigitError("init takes no arguments");
  }
  const fs::path directory = fs::current_path();
  const bool existed = fs::exists(directory / ".minigit");
  minigit::Repository::init(directory);
  std::cout << (existed ? "Reinitialized" : "Initialized")
            << " empty MiniGit repository in "
            << (directory / ".minigit").string() << "\n";
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

int commandLog(const std::vector<std::string>& args) {
  bool showGraph = false;
  for (std::size_t i = 1; i < args.size(); ++i) {
    if (args[i] == "--graph") {
      showGraph = true;
    } else {
      throw MinigitError("log: unknown option '" + args[i] + "'");
    }
  }

  const minigit::Repository repo = minigit::Repository::open();
  const auto head = repo.headCommit();
  if (!head.has_value()) {
    throw MinigitError("your current branch '" +
                       (repo.headBranch().empty() ? "HEAD"
                                                  : repo.headBranch()) +
                       "' does not have any commits yet");
  }

  const auto entries = minigit::walkHistory(repo.objects(), *head);

  if (showGraph) {
    std::vector<minigit::GraphNode> nodes;
    nodes.reserve(entries.size());
    for (const auto& entry : entries) {
      minigit::GraphNode node;
      node.id = entry.oid.hex();
      node.parents.reserve(entry.commit.parents.size());
      for (const minigit::Oid& parent : entry.commit.parents) {
        node.parents.push_back(parent.hex());
      }
      node.label = entry.oid.shortHex() + " " + entry.commit.summary();
      nodes.push_back(std::move(node));
    }
    for (const std::string& line : minigit::renderAsciiGraph(nodes)) {
      std::cout << line << '\n';
    }
    return 0;
  }

  for (const auto& entry : entries) {
    std::cout << "commit " << entry.oid.hex() << '\n';
    if (entry.commit.parents.size() > 1) {
      std::cout << "Merge:";
      for (const minigit::Oid& parent : entry.commit.parents) {
        std::cout << ' ' << parent.shortHex();
      }
      std::cout << '\n';
    }
    std::cout << "Author: " << entry.commit.author << '\n'
              << "Date:   " << formatTimestamp(entry.commit.timestamp) << '\n'
              << '\n';

    std::istringstream message(entry.commit.message);
    std::string line;
    while (std::getline(message, line)) {
      std::cout << "    " << line << '\n';
    }
    std::cout << '\n';
  }
  return 0;
}

int commandDiff(const std::vector<std::string>& args) {
  bool staged = false;
  minigit::DiffAlgorithm algorithm = minigit::DiffAlgorithm::Lcs;

  for (std::size_t i = 1; i < args.size(); ++i) {
    if (args[i] == "--staged" || args[i] == "--cached") {
      staged = true;
    } else if (args[i] == "--myers") {
      algorithm = minigit::DiffAlgorithm::Myers;
    } else {
      throw MinigitError("diff: unknown option '" + args[i] + "'");
    }
  }

  const minigit::Repository repo = minigit::Repository::open();
  for (const minigit::FileDiff& fileDiff : repo.diff(staged, algorithm)) {
    std::cout << fileDiff.text;
  }
  return 0;
}

int commandMerge(const std::vector<std::string>& args) {
  std::string target;
  std::string message;

  for (std::size_t i = 1; i < args.size(); ++i) {
    if (args[i] == "-m" || args[i] == "--message") {
      if (i + 1 >= args.size()) {
        throw MinigitError("merge: option '" + args[i] +
                           "' requires a message argument");
      }
      message = args[++i];
    } else if (target.empty()) {
      target = args[i];
    } else {
      throw MinigitError("merge: unexpected argument '" + args[i] + "'");
    }
  }

  if (target.empty()) {
    throw MinigitError("merge requires a branch name");
  }

  minigit::Repository repo = minigit::Repository::open();
  const minigit::MergeReport report = repo.merge(target, message);

  switch (report.kind) {
    case minigit::MergeKind::UpToDate:
      std::cout << "Already up to date.\n";
      return 0;
    case minigit::MergeKind::FastForward:
      std::cout << "Fast-forward to " << report.commitId.shortHex() << '\n';
      return 0;
    case minigit::MergeKind::Merged:
      std::cout << "Merge made by the three-way strategy: ["
                << repo.headBranch() << ' ' << report.commitId.shortHex()
                << "] "
                << (message.empty() ? "Merge branch '" + target + "'"
                                    : message)
                << '\n';
      return 0;
    case minigit::MergeKind::Conflicts:
      std::cout << "Merge base " << report.base.shortHex() << '\n'
                << "Automatic merge failed; fix conflicts and then commit "
                   "the result.\n"
                << "Conflicts:\n";
      for (const std::string& path : report.conflicts) {
        std::cout << "\tboth modified: " << path << '\n';
      }
      return 1;
  }
  return 0;
}

int commandBranch(const std::vector<std::string>& args) {
  minigit::Repository repo = minigit::Repository::open();

  if (args.size() == 1) {
    const std::string current = repo.headBranch();
    for (const auto& [name, oid] : repo.branches()) {
      (void)oid;
      std::cout << (name == current ? "* " : "  ") << name << '\n';
    }
    return 0;
  }

  if (args.size() > 2) {
    throw MinigitError("branch takes at most one name");
  }

  repo.createBranch(args[1]);
  std::cout << "Created branch '" << args[1] << "'\n";
  return 0;
}

int commandCheckout(const std::vector<std::string>& args) {
  if (args.size() != 2) {
    throw MinigitError("checkout requires exactly one <branch|commit>");
  }

  minigit::Repository repo = minigit::Repository::open();
  repo.checkout(args[1]);

  if (repo.headBranch().empty()) {
    std::cout << "Note: switching to '" << args[1] << "'.\n"
              << "You are in 'detached HEAD' state; commits made here will "
                 "not move any branch.\n";
  } else {
    std::cout << "Switched to branch '" << repo.headBranch() << "'\n";
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
  if (command == "log") {
    return commandLog(args);
  }
  if (command == "branch") {
    return commandBranch(args);
  }
  if (command == "checkout") {
    return commandCheckout(args);
  }
  if (command == "diff") {
    return commandDiff(args);
  }
  if (command == "merge") {
    return commandMerge(args);
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
