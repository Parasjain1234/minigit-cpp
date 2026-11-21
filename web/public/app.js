"use strict";

const PALETTE = ["#79f2a8", "#8ab4ff", "#ffd479", "#c79bff", "#ff9db0", "#6fe3d4"];

const state = {
  data: null,
  layout: null,
};

/* ---------------------------------------------------------------- data --- */

async function loadSnapshot() {
  try {
    const response = await fetch("./data.json", { cache: "no-store" });
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}`);
    }
    state.data = await response.json();
  } catch (error) {
    console.warn("Snapshot unavailable:", error);
    document.getElementById("data-error").hidden = false;
    return;
  }
  renderStats();
  buildLayout();
  renderGraph();
  renderBranches();
  selectCommit(state.data.commits[0]?.id);
}

function renderStats() {
  const data = state.data;
  if (!data) return;

  const counts = data.objectCounts || {};
  const objects = (counts.blobs || 0) + (counts.trees || 0) + (counts.commits || 0);
  const entries = [
    [data.commits.length, "commits in the DAG"],
    [data.branches.length, "branches"],
    [data.files.length, "tracked files"],
    [objects, "stored objects"],
  ];

  const container = document.getElementById("hero-stats");
  container.innerHTML = entries
    .map(
      ([value, label]) =>
        `<div class="stat"><strong>${value}</strong><span>${label}</span></div>`
    )
    .join("");
}

/* ----------------------------------------------------------------- DAG --- */

function buildLayout() {
  const commits = state.data.commits;
  const lanes = [];
  const nodes = new Map();
  const rows = [];

  commits.forEach((commit, row) => {
    let column = lanes.indexOf(commit.id);
    if (column === -1) {
      column = lanes.indexOf(null);
      if (column === -1) {
        lanes.push(null);
        column = lanes.length - 1;
      }
    }
    lanes[column] = commit.id;

    nodes.set(commit.id, { row, column, commit });
    rows.push({ commit, column, laneCount: lanes.length });

    // A lane that reaches a commit drawn elsewhere is closed.
    for (let i = 0; i < lanes.length; i += 1) {
      if (i !== column && lanes[i] === commit.id) {
        lanes[i] = null;
      }
    }

    if (commit.parents.length > 0) {
      lanes[column] = commit.parents[0];
    } else {
      lanes[column] = null;
    }

    for (let p = 1; p < commit.parents.length; p += 1) {
      let lane = lanes.indexOf(commit.parents[p]);
      if (lane === -1) {
        lane = lanes.indexOf(null);
        if (lane === -1) {
          lanes.push(null);
          lane = lanes.length - 1;
        }
        lanes[lane] = commit.parents[p];
      }
    }
  });

  let maxColumn = 0;
  for (const node of nodes.values()) {
    maxColumn = Math.max(maxColumn, node.column);
  }

  state.layout = { nodes, rows, maxColumn };
}

function renderGraph() {
  const layout = state.layout;
  const svg = document.getElementById("dag");
  if (!layout) return;

  const rowHeight = 62;
  const columnWidth = 46;
  const padLeft = 34;
  const padTop = 34;
  const labelX = padLeft + (layout.maxColumn + 1) * columnWidth + 18;
  const width = Math.max(720, labelX + 340);
  const height = padTop * 2 + Math.max(1, layout.rows.length - 1) * rowHeight;

  svg.setAttribute("viewBox", `0 0 ${width} ${height}`);
  svg.setAttribute("width", width);
  svg.setAttribute("height", height);
  svg.innerHTML = "";

  const x = (column) => padLeft + column * columnWidth;
  const y = (row) => padTop + row * rowHeight;

  const edgeLayer = document.createElementNS("http://www.w3.org/2000/svg", "g");
  const nodeLayer = document.createElementNS("http://www.w3.org/2000/svg", "g");
  const labelLayer = document.createElementNS("http://www.w3.org/2000/svg", "g");
  svg.append(edgeLayer, nodeLayer, labelLayer);

  for (const node of layout.nodes.values()) {
    const parentNodeColor = PALETTE[node.column % PALETTE.length];

    for (const parentId of node.commit.parents) {
      const parent = layout.nodes.get(parentId);
      if (!parent) continue;

      const path = document.createElementNS("http://www.w3.org/2000/svg", "path");
      const startX = x(node.column);
      const startY = y(node.row);
      const endX = x(parent.column);
      const endY = y(parent.row);

      let d;
      if (node.column === parent.column) {
        d = `M ${startX} ${startY} L ${endX} ${endY}`;
      } else {
        const midY = (startY + endY) / 2;
        d = `M ${startX} ${startY} C ${startX} ${midY}, ${endX} ${midY}, ${endX} ${endY}`;
      }

      path.setAttribute("d", d);
      path.setAttribute("fill", "none");
      path.setAttribute("stroke", parentNodeColor);
      path.setAttribute("stroke-opacity", "0.55");
      path.setAttribute("stroke-width", "1.6");
      edgeLayer.appendChild(path);
    }
  }

  for (const node of layout.nodes.values()) {
    const circle = document.createElementNS("http://www.w3.org/2000/svg", "circle");
    circle.setAttribute("cx", x(node.column));
    circle.setAttribute("cy", y(node.row));
    circle.setAttribute("r", "6.5");
    circle.setAttribute("fill", "#0b1210");
    circle.setAttribute("stroke", PALETTE[node.column % PALETTE.length]);
    circle.setAttribute("stroke-width", "2.2");
    circle.dataset.commit = node.commit.id;
    circle.addEventListener("click", () => selectCommit(node.commit.id));

    const title = document.createElementNS("http://www.w3.org/2000/svg", "title");
    title.textContent = `${node.commit.id.slice(0, 7)} ${firstLine(node.commit.message)}`;
    circle.appendChild(title);
    nodeLayer.appendChild(circle);

    const hash = document.createElementNS("http://www.w3.org/2000/svg", "text");
    hash.setAttribute("x", labelX);
    hash.setAttribute("y", y(node.row) + 4);
    hash.setAttribute("class", "hash");
    hash.textContent = node.commit.id.slice(0, 7);
    labelLayer.appendChild(hash);

    const label = document.createElementNS("http://www.w3.org/2000/svg", "text");
    label.setAttribute("x", labelX + 58);
    label.setAttribute("y", y(node.row) + 4);
    label.textContent = truncate(firstLine(node.commit.message), 34);
    labelLayer.appendChild(label);
  }
}

function renderBranches() {
  const list = document.getElementById("branch-list");
  const current = state.data.branch;
  list.innerHTML = state.data.branches
    .map((branch) => {
      const isCurrent = branch.name === current;
      return `<li>
        <span class="${isCurrent ? "current" : ""}">${isCurrent ? "* " : "  "}${escapeHtml(branch.name)}</span>
        <span class="sha">${branch.commit.slice(0, 7)}</span>
      </li>`;
    })
    .join("");
}

function selectCommit(id) {
  if (!id || !state.data) return;
  const commit = state.data.commits.find((entry) => entry.id === id);
  if (!commit) return;

  const detail = document.getElementById("commit-detail");
  const branches = state.data.branches
    .filter((branch) => branch.commit === commit.id)
    .map((branch) => branch.name);

  detail.innerHTML = `
    <h4>${escapeHtml(firstLine(commit.message))}</h4>
    <p class="sha-line">${commit.id}</p>
    <p class="meta">${escapeHtml(commit.author)}<br>${formatDate(commit.timestamp)}</p>
    <p class="meta">parents: ${
      commit.parents.length
        ? commit.parents.map((parent) => parent.slice(0, 7)).join(", ")
        : "none (root)"
    }</p>
    ${branches.length ? `<p class="meta">branches: ${escapeHtml(branches.join(", "))}</p>` : ""}
  `;
}

/* --------------------------------------------------------------- helpers --- */

function firstLine(text) {
  const index = text.indexOf("\n");
  return index === -1 ? text : text.slice(0, index);
}

function truncate(text, max) {
  return text.length > max ? `${text.slice(0, max - 1)}…` : text;
}

function formatDate(timestamp) {
  const date = new Date(timestamp * 1000);
  return date.toUTCString();
}

function escapeHtml(text) {
  return String(text)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;");
}

/* ---------------------------------------------------------- diff engine --- */

function splitLines(text) {
  const lines = [];
  let start = 0;
  while (start < text.length) {
    const newline = text.indexOf("\n", start);
    if (newline === -1) {
      lines.push(text.slice(start));
      break;
    }
    lines.push(text.slice(start, newline));
    start = newline + 1;
  }
  return lines;
}

/// LCS dynamic programming: build the table, then backtrack it.
function lcsDiff(oldLines, newLines) {
  const rows = oldLines.length;
  const columns = newLines.length;
  const width = columns + 1;
  const table = new Uint32Array((rows + 1) * width);
  const at = (i, j) => i * width + j;

  for (let i = 1; i <= rows; i += 1) {
    for (let j = 1; j <= columns; j += 1) {
      table[at(i, j)] =
        oldLines[i - 1] === newLines[j - 1]
          ? table[at(i - 1, j - 1)] + 1
          : Math.max(table[at(i - 1, j)], table[at(i, j - 1)]);
    }
  }

  const edits = [];
  let i = rows;
  let j = columns;
  while (i > 0 && j > 0) {
    if (oldLines[i - 1] === newLines[j - 1]) {
      edits.push({ kind: "equal", line: oldLines[i - 1] });
      i -= 1;
      j -= 1;
    } else if (table[at(i - 1, j)] >= table[at(i, j - 1)]) {
      edits.push({ kind: "delete", line: oldLines[i - 1] });
      i -= 1;
    } else {
      edits.push({ kind: "insert", line: newLines[j - 1] });
      j -= 1;
    }
  }
  while (i > 0) {
    edits.push({ kind: "delete", line: oldLines[i - 1] });
    i -= 1;
  }
  while (j > 0) {
    edits.push({ kind: "insert", line: newLines[j - 1] });
    j -= 1;
  }

  edits.reverse();

  // Keep the familiar "remove, then add" order inside each changed run.
  let runStart = 0;
  while (runStart < edits.length) {
    if (edits[runStart].kind === "equal") {
      runStart += 1;
      continue;
    }
    let runEnd = runStart;
    while (runEnd < edits.length && edits[runEnd].kind !== "equal") {
      runEnd += 1;
    }
    const run = edits.slice(runStart, runEnd);
    const deletes = run.filter((edit) => edit.kind === "delete");
    const inserts = run.filter((edit) => edit.kind === "insert");
    edits.splice(runStart, runEnd - runStart, ...deletes, ...inserts);
    runStart = runEnd;
  }

  return { edits, cells: (rows + 1) * (columns + 1) };
}

/// Myers' greedy furthest-reaching-path diff (port of src/diff/myers.cpp).
function myersDiff(oldLines, newLines) {
  const n = oldLines.length;
  const m = newLines.length;
  const maxDistance = n + m;
  const offset = maxDistance + 1;
  const v = new Int32Array(2 * maxDistance + 3);
  const trace = [];

  let finalDistance = 0;
  let finished = false;

  for (let d = 0; d <= maxDistance && !finished; d += 1) {
    trace.push(v.slice());
    for (let k = -d; k <= d; k += 2) {
      const index = offset + k;
      let x;
      if (k === -d || (k !== d && v[index - 1] < v[index + 1])) {
        x = v[index + 1];
      } else {
        x = v[index - 1] + 1;
      }
      let y = x - k;
      while (x < n && y < m && oldLines[x] === newLines[y]) {
        x += 1;
        y += 1;
      }
      v[index] = x;
      if (x >= n && y >= m) {
        finalDistance = d;
        finished = true;
        break;
      }
    }
  }

  const edits = [];
  let x = n;
  let y = m;
  for (let d = finalDistance; d > 0; d -= 1) {
    const previous = trace[d];
    const k = x - y;
    let previousK;
    if (k === -d || (k !== d && previous[offset + k - 1] < previous[offset + k + 1])) {
      previousK = k + 1;
    } else {
      previousK = k - 1;
    }
    const previousX = previous[offset + previousK];
    const previousY = previousX - previousK;
    while (x > previousX && y > previousY) {
      edits.push({ kind: "equal", line: oldLines[x - 1] });
      x -= 1;
      y -= 1;
    }
    if (x === previousX) {
      edits.push({ kind: "insert", line: newLines[y - 1] });
      y -= 1;
    } else {
      edits.push({ kind: "delete", line: oldLines[x - 1] });
      x -= 1;
    }
  }
  while (x > 0 && y > 0) {
    edits.push({ kind: "equal", line: oldLines[x - 1] });
    x -= 1;
    y -= 1;
  }

  edits.reverse();
  return { edits, distance: finalDistance };
}

function renderEdits(edits) {
  return edits
    .map((edit) => {
      const prefix = edit.kind === "insert" ? "+" : edit.kind === "delete" ? "-" : " ";
      const cls = edit.kind === "insert" ? "line-add" : edit.kind === "delete" ? "line-del" : "line-ctx";
      return `<span class="${cls}">${prefix} ${escapeHtml(edit.line) || " "}</span>`;
    })
    .join("");
}

function runDiff() {
  const beforeLines = splitLines(document.getElementById("before").value);
  const afterLines = splitLines(document.getElementById("after").value);
  const algorithm = document.getElementById("algo").value;

  if (beforeLines.length * afterLines.length > 4000000) {
    document.getElementById("diff-stats").textContent =
      "input too large for the browser demo";
    return;
  }

  const started = performance.now();
  const result = algorithm === "myers" ? myersDiff(beforeLines, afterLines) : lcsDiff(beforeLines, afterLines);
  const elapsed = performance.now() - started;

  const changed = result.edits.filter((edit) => edit.kind !== "equal").length;
  document.getElementById("diff-output").innerHTML = renderEdits(result.edits);

  const stats = document.getElementById("diff-stats");
  if (algorithm === "myers") {
    stats.textContent = `Myers · edit distance ${result.distance} · ${changed} changed lines · ${elapsed.toFixed(2)} ms`;
  } else {
    stats.textContent = `LCS · table ${beforeLines.length}×${afterLines.length} = ${result.cells.toLocaleString()} cells · ${changed} changed lines · ${elapsed.toFixed(2)} ms`;
  }
}

/* ----------------------------------------------------------------- boot --- */

function seedEditors() {
  document.getElementById("before").value = [
    "#include <iostream>",
    "",
    "int main() {",
    '    std::cout << "hello minigit\\n";',
    "}",
    "",
  ].join("\n");

  document.getElementById("after").value = [
    "#include <iostream>",
    "",
    "int main() {",
    '    std::cout << "hello minigit\\n";',
    '    std::cout << "version 0.1\\n";',
    "}",
    "",
  ].join("\n");
}

document.getElementById("run-diff").addEventListener("click", runDiff);
document.getElementById("algo").addEventListener("change", runDiff);

seedEditors();
runDiff();
loadSnapshot();
