# MiniGit web UI

A dependency-free static site that visualises a MiniGit repository:

- the commit DAG rendered as lanes from a real `minigit export` snapshot,
- a live LCS vs Myers diff playground (the CLI algorithms ported to JS),
- the object format and module map.

## Run locally

The page fetches `data.json`, so it needs an HTTP origin:

```bash
cd web/public
python -m http.server 4173
# open http://localhost:4173
```

## Use your own repository

```bash
cd /path/to/your/repo
minigit export > data.json
cp data.json /path/to/minigit/web/public/data.json
```

## Refresh the bundled demo snapshot

The committed `public/data.json` was produced by running the CLI on a small
demo repository with a merge and three branches.

## Deploy

`worker.js` serves the `public/` directory on Cloudflare Workers:

```bash
# from the repository root, with a Sites project configured
sites_deploy web/deploy.json
```
