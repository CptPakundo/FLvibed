// The Ghidra MCP server for Claude Code (registered in the environment's .mcp.json by tools\claude\install.ps1):
// makes sure the headless Ghidra server (GhidraMCP 7.0.0's GhidraMCPHeadlessServer on the FLvibed project,
// loopback 127.0.0.1:8089, a bearer token) runs - starting it detached when it does not answer - then runs the MCP
// bridge on this process's stdio. Everything lives in research\ghidra\ (git-ignored; tools\ghidra\README.md).
// `node mcp_launch.js --server-only` starts / checks the server and exits (ghidra.ps1 -Serve).
'use strict';
const fs = require('fs');
const path = require('path');
const http = require('http');
const crypto = require('crypto');
const { spawn } = require('child_process');

const root = path.resolve(__dirname, '..', '..');
const g = path.join(root, 'research', 'ghidra');
const port = 8089;
const log = (m) => process.stderr.write(`ghidra-mcp launch: ${m}\n`);

function firstDir(p) {
    const d = fs.readdirSync(p, { withFileTypes: true }).find((e) => e.isDirectory());
    if (!d) throw new Error(`nothing in ${p}`);
    return path.join(p, d.name);
}
function jars(dir) {
    const out = [];
    for (const m of fs.readdirSync(dir, { withFileTypes: true })) {
        const lib = path.join(dir, m.name, 'lib');
        if (m.isDirectory() && fs.existsSync(lib))
            for (const f of fs.readdirSync(lib)) if (f.endsWith('.jar')) out.push(path.join(lib, f));
    }
    return out;
}
function token() {
    const f = path.join(g, '.token');
    try { // exclusive create: two launchers starting together must not each write their own token
        fs.writeFileSync(f, crypto.randomBytes(32).toString('hex'), { flag: 'wx' });
    } catch (e) {
        if (e.code !== 'EEXIST') throw e;
    }
    return fs.readFileSync(f, 'utf8').trim();
}
function check() {
    return new Promise((resolve) => {
        const req = http.get({ host: '127.0.0.1', port, path: '/check_connection', timeout: 2000 }, (res) => {
            let s = '';
            res.on('data', (d) => (s += d)).on('end', () => resolve(res.statusCode === 200 && /"ok"/.test(s)));
        });
        req.on('error', () => resolve(false)).on('timeout', () => (req.destroy(), resolve(false)));
    });
}

async function ensureServer(tok) {
    if (await check()) return;
    const gh = firstDir(path.join(g, 'ghidra_12.1.4'));
    const java = path.join(firstDir(path.join(g, 'jdk21')), 'bin', 'java.exe');
    const cp = [path.join(g, 'dl_ghidramcp_7.0.0', 'x', 'GhidraMCP', 'lib', 'GhidraMCP-7.0.0.jar')];
    for (const part of ['Framework', 'Features', 'Processors']) cp.push(...jars(path.join(gh, 'Ghidra', part)));
    const project = path.join(g, 'project', 'FLvibed.gpr');
    fs.mkdirSync(path.join(g, 'logs'), { recursive: true });
    const out = fs.openSync(path.join(g, 'logs', 'server.log'), 'a');
    const args = ['-Xmx4g', '-XX:+UseG1GC', `-Dghidra.home=${gh}`, '-Dapplication.name=GhidraMCP', '-cp', cp.join(';'),
        'com.xebyte.headless.GhidraMCPHeadlessServer', '--bind', '127.0.0.1', '--port', String(port), '--project', project];
    const env = { ...process.env, GHIDRA_MCP_AUTH_TOKEN: tok, GHIDRA_MCP_FILE_ROOT: g };
    const child = spawn(java, args, { detached: true, stdio: ['ignore', out, out], env, windowsHide: true });
    child.unref();
    log(`started the headless server (pid ${child.pid}), log research\\ghidra\\logs\\server.log`);
    for (let i = 0; i < 240; i++) {
        if (await check()) return;
        await new Promise((r) => setTimeout(r, 500));
    }
    throw new Error('the headless server did not answer in 120 s (research\\ghidra\\logs\\server.log)');
}

module.exports = { ensureServer, token, port };
if (require.main === module) (async () => {
    const tok = token();
    await ensureServer(tok);
    if (process.argv.includes('--server-only')) return log(`server ready at http://127.0.0.1:${port}`);
    const bridge = path.join(g, 'bridge_venv', 'Scripts', 'bridge-mcp-ghidra.exe');
    const env = { ...process.env, GHIDRA_MCP_URL: `http://127.0.0.1:${port}`, GHIDRA_MCP_AUTH_TOKEN: tok,
        GHIDRA_MCP_REQUIRE_PROGRAM_SELECTORS: '1' };
    const b = spawn(bridge, ['--transport', 'stdio'], { stdio: 'inherit', env, windowsHide: true });
    b.on('exit', (code) => process.exit(code ?? 1));
})().catch((e) => {
    log(e.message);
    process.exit(1);
});
