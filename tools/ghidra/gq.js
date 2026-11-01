// gq - Ghidra queries from the shell, over the headless GhidraMCP server's HTTP API (the same server the MCP bridge
// uses; started on demand by mcp_launch.js). For the agents that have only a shell (game-facts) and for sessions
// started before the MCP registration. Read-only except `mkfn` / `name` / `note`, which change only the research
// project (research\ghidra\project; never an install).
//   node gq.js <program> dec <fn>          decompiled C (fn: name, 0x<va>, or rva:0x<rva>)
//   node gq.js <program> asm <fn>          the function's disassembly
//   node gq.js <program> callers <fn>      its callers / callees
//   node gq.js <program> xto <addr> [n]    references to an address (xfrom: from it)
//   node gq.js <program> find <pattern>    functions whose name contains it
//   node gq.js <program> str <regex>       defined strings
//   node gq.js <program> ins <mnemonic> <operand substring> [fn]   instructions (e.g. "mov" "[ecx+0xd0]")
//   node gq.js <program> mem <addr> [len]  raw bytes
//   node gq.js <program> mkfn <addr> [name]   create a function analysis missed (vtable-only code)
//   node gq.js <program> name <fn> <new>   rename a function; note <addr> <text>: a plate comment
//   node gq.js raw GET|POST <endpoint> [k=v ...]  any endpoint, named without the slash under Git Bash
//                                         (`raw GET mcp/schema` lists all 190)
// <program>: exe | common | content | server | ... or the file name (Freelancer.exe, Common.dll).
'use strict';
const http = require('http');
const { ensureServer, token, port } = require('./mcp_launch.js');

const ALIAS = { exe: 'Freelancer.exe', freelancer: 'Freelancer.exe' };
const program = (p) => ALIAS[p.toLowerCase()] || (/\.(dll|exe)$/i.test(p) ? p : p.charAt(0).toUpperCase() + p.slice(1) + '.dll');

function call(method, path, params, tok) {
    return new Promise((resolve, reject) => {
        const q = method === 'GET' ? '?' + new URLSearchParams(params).toString() : '';
        const body = method === 'POST' ? JSON.stringify(params) : null;
        const req = http.request({ host: '127.0.0.1', port, path: path + q, method, timeout: 300000,
            headers: { Authorization: `Bearer ${tok}`, ...(body ? { 'Content-Type': 'application/json' } : {}) } }, (res) => {
            let s = '';
            res.setEncoding('utf8');
            res.on('data', (d) => (s += d)).on('end', () => resolve({ status: res.statusCode, text: s }));
        });
        req.on('error', reject).on('timeout', () => req.destroy(new Error('timeout')));
        if (body) req.write(body);
        req.end();
    });
}

async function base(prog, tok) {
    const r = JSON.parse((await call('GET', '/get_metadata', { program: prog }, tok)).text);
    return parseInt(r.base_address, 16);
}
async function ref(prog, s, tok) { // rva:0x619D0 -> 0x<va>
    const m = /^rva:(0x[0-9a-f]+|\d+)$/i.exec(s);
    return m ? '0x' + (await base(prog, tok) + Number(m[1])).toString(16).toUpperCase() : s;
}

function show(r, key) {
    let j;
    try { j = JSON.parse(r.text); } catch { return console.log(r.text); }
    if (key && j && typeof j === 'object') {
        const v = j[key] ?? (j.result && j.result[key]);
        if (typeof v === 'string') return console.log(v);
    }
    console.log(JSON.stringify(j, null, 1));
}

(async () => {
    const [p, cmd, ...a] = process.argv.slice(2);
    if (!p || !cmd) return console.error('usage: node gq.js <program> <dec|asm|callers|xto|xfrom|find|str|ins|mem|mkfn|name|note> ... (header)');
    const tok = token();
    await ensureServer(tok);
    if (p === 'raw') {
        const params = Object.fromEntries(a.slice(1).map((kv) => [kv.slice(0, kv.indexOf('=')), kv.slice(kv.indexOf('=') + 1)]));
        // "get_metadata" or "/get_metadata" (Git Bash turns a leading "/x" into a Windows path: give it without the slash)
        const ep = '/' + String(a[0] || '').replace(/^[A-Za-z]:\/.*?\/Git\//i, '').replace(/^\/+/, '');
        return show(await call(cmd.toUpperCase(), ep, params, tok));
    }
    const prog = program(p);
    const at = a[0] ? await ref(prog, a[0], tok) : '';
    switch (cmd) {
        case 'dec': {
            const r = await call('GET', '/get_functions', { program: prog, function: at, fields: 'signature,entry_point,plate_comment,decompiled_code' }, tok);
            let j; try { j = JSON.parse(r.text); } catch { return console.log(r.text); }
            if (j.error) return console.log(j.error + (/not found/i.test(j.error) ? `  (code without a function: node gq.js ${p} mkfn ${at})` : ''));
            const f = j.function || j;
            console.log(`// ${prog} ${f.entry_point || at}  ${f.signature || ''}`); // (the plate comment: in the code)
            return console.log(f.decompiled_code || JSON.stringify(f, null, 1));
        }
        case 'asm': return show(await call('GET', '/disassemble_function', { program: prog, address: at }, tok), 'disassembly');
        case 'callers': return show(await call('GET', '/get_functions', { program: prog, function: at, fields: 'signature,callers,callees' }, tok));
        case 'xto': return show(await call('GET', '/get_xrefs_to', { program: prog, address: at, limit: a[1] || 100 }, tok));
        case 'xfrom': return show(await call('GET', '/get_xrefs_from', { program: prog, address: at, limit: a[1] || 100 }, tok));
        case 'find': return show(await call('GET', '/find_functions', { program: prog, name_pattern: a[0], limit: 200 }, tok));
        case 'str': return show(await call('GET', '/search_strings', { program: prog, search_term: a[0], limit: 100 }, tok));
        case 'ins': return show(await call('GET', '/search_instructions', { program: prog, mnemonic: a[0] || '', operand_pattern: a[1] || '', function: a[2] ? await ref(prog, a[2], tok) : '', limit: 200 }, tok));
        case 'mem': return show(await call('GET', '/read_memory', { program: prog, address: at, length: a[1] || 64 }, tok));
        case 'mkfn': return show(await call('POST', '/create_function?program=' + encodeURIComponent(prog), { address: at, name: a[1] || '', disassemble_first: true }, tok));
        case 'name': {
            const r = JSON.parse((await call('GET', '/get_functions', { program: prog, function: at, fields: 'signature' }, tok)).text);
            const old = (r.function || r).name || (r.signature || '').replace(/^.*?(\w+)\(.*$/, '$1');
            return show(await call('POST', '/rename_function?program=' + encodeURIComponent(prog), { old_name: old, new_name: a[1] }, tok));
        }
        case 'note': return show(await call('POST', '/set_comment?program=' + encodeURIComponent(prog), { address: at, comment: a.slice(1).join(' '), type: 'plate' }, tok));
        default: console.error('unknown command ' + cmd);
    }
})().catch((e) => { console.error('gq: ' + e.message); process.exit(1); });
