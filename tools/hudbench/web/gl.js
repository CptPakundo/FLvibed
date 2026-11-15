/* The display list's WebGL replay (tools\hudbench\FORMAT.md): the kit's draw calls 1:1, with the kit's device state
   (core\ui\hud2_kit.cpp End): triangle lists, blend SRCALPHA / INVSRCALPHA with ADD (alpha blended the same way, as
   D3D8 has no separate alpha blend), no depth, no culling, linear filtering, clamped coordinates, no mip maps; COLOROP
   modulate (texture x vertex colour) or, for the icon layers, the vertex colour alone with ALPHAOP modulate (texture
   alpha x vertex alpha); a pass without alpha writes masks them (COLORWRITEENABLE 0x7). Textures keep straight alpha.
   The replay draws into an offscreen RGBA target whose row 0 is the list's top row (no y flip), so a triangle edge
   through a pixel centre falls the way D3D's top-left rule puts it; the target keeps its alpha (cleared to 0, so a
   pass that writes none leaves it at 0) and is copied 1:1 (nearest) onto the opaque canvas, flipped for display.
   A plain script: window.FLGL. Needs fldl.js. */
(function (root) {
    'use strict';
    const FLDL = root.FLDL;
    const VS = `
attribute vec2 a_pos; attribute vec4 a_col; attribute vec2 a_uv; attribute float a_slot;
uniform vec2 u_list; uniform vec2 u_target; uniform float u_stretch;
varying vec4 v_col; varying vec2 v_uv; varying float v_slot;
void main() {
  vec2 p = a_pos + 0.5;                   // D3D pixel centres on integers, GL's on halves
  if (u_stretch > 0.5) p = p * (u_target / u_list);
  gl_Position = vec4(p / u_target * 2.0 - 1.0, 0.0, 1.0);   // list row 0 = the target's row 0
  v_col = a_col; v_uv = a_uv; v_slot = a_slot;
}`;
    const FS = `
precision highp float;
uniform sampler2D u_atlas; uniform float u_icon; uniform float u_hasAtlas;
varying vec4 v_col; varying vec2 v_uv; varying float v_slot;
void main() {
  vec4 t = vec4(1.0);
  if (v_slot > 0.5 && u_hasAtlas > 0.5) t = texture2D(u_atlas, v_uv);
  gl_FragColor = u_icon > 0.5 ? vec4(v_col.rgb, v_col.a * t.a) : v_col * t;
}`;
    // a full-target quad: u_flip 0 maps the image's top row to the target's row 0 (the background into the replay
    // target), 1 to the canvas' top (the replay target onto the canvas)
    const QUAD_VS = `attribute vec2 a_p; uniform float u_flip; varying vec2 v_t;
void main() { v_t = vec2(a_p.x * 0.5 + 0.5, u_flip > 0.5 ? 0.5 - a_p.y * 0.5 : 0.5 + a_p.y * 0.5); gl_Position = vec4(a_p, 0.0, 1.0); }`;
    const QUAD_FS = `precision highp float; uniform sampler2D u_img; varying vec2 v_t; void main() { gl_FragColor = texture2D(u_img, v_t); }`;

    function compile(gl, vs, fs) {
        const p = gl.createProgram();
        for (const [type, src] of [[gl.VERTEX_SHADER, vs], [gl.FRAGMENT_SHADER, fs]]) {
            const s = gl.createShader(type);
            gl.shaderSource(s, src);
            gl.compileShader(s);
            if (!gl.getShaderParameter(s, gl.COMPILE_STATUS)) throw new Error('shader: ' + gl.getShaderInfoLog(s));
            gl.attachShader(p, s);
        }
        gl.linkProgram(p);
        if (!gl.getProgramParameter(p, gl.LINK_STATUS)) throw new Error('program: ' + gl.getProgramInfoLog(p));
        return p;
    }
    function texture(gl, source, filter) {
        const t = gl.createTexture();
        gl.bindTexture(gl.TEXTURE_2D, t);
        gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, false);
        gl.pixelStorei(gl.UNPACK_COLORSPACE_CONVERSION_WEBGL, gl.NONE);
        gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, false);
        if (source && source.width !== undefined && !(source instanceof Uint8Array)) gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, source);
        else gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, source.w, source.h, 0, gl.RGBA, gl.UNSIGNED_BYTE, null);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, filter || gl.LINEAR);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, filter || gl.LINEAR);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
        return t;
    }

    function Renderer(canvas) {
        const gl = canvas.getContext('webgl', { alpha: false, premultipliedAlpha: false, antialias: false, depth: false,
            stencil: false, preserveDrawingBuffer: true });
        if (!gl) throw new Error('this browser gives no WebGL context');
        this.canvas = canvas;
        this.gl = gl;
        this.prog = compile(gl, VS, FS);
        this.quadProg = compile(gl, QUAD_VS, QUAD_FS);
        this.loc = {};
        for (const a of ['a_pos', 'a_col', 'a_uv', 'a_slot']) this.loc[a] = gl.getAttribLocation(this.prog, a);
        for (const u of ['u_list', 'u_target', 'u_stretch', 'u_atlas', 'u_icon', 'u_hasAtlas']) this.loc[u] = gl.getUniformLocation(this.prog, u);
        this.buf = { pos: gl.createBuffer(), col: gl.createBuffer(), uv: gl.createBuffer(), slot: gl.createBuffer(), quad: gl.createBuffer() };
        gl.bindBuffer(gl.ARRAY_BUFFER, this.buf.quad);
        gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1, -1, 1, -1, -1, 1, -1, 1, 1, -1, 1, 1]), gl.STATIC_DRAW);
        this.maxSide = Math.min(gl.getParameter(gl.MAX_TEXTURE_SIZE), gl.getParameter(gl.MAX_RENDERBUFFER_SIZE));
        this.atlas = null;     // the manifest
        this.atlasTex = null;
        this.bgTex = null;
        this.target = null;    // {fb, tex, w, h}
    }

    /* The atlas: its manifest (FLDL.parseAtlas) and its image (an ImageBitmap / image / canvas), or nulls to drop it */
    Renderer.prototype.setAtlas = function (manifest, image) {
        const gl = this.gl;
        if (this.atlasTex) gl.deleteTexture(this.atlasTex);
        this.atlasTex = null;
        this.atlas = null;
        if (!manifest || !image) return;
        if (image.width !== manifest.size[0] || image.height !== manifest.size[1])
            throw new Error(`the atlas image is ${image.width}x${image.height}, its manifest says ${manifest.size.join('x')}`);
        this.atlasTex = texture(gl, image);
        this.atlas = manifest;
    };
    /* An optional background image drawn over the whole target first (later: the 'under' capture of level 2) */
    Renderer.prototype.setBackground = function (image) {
        const gl = this.gl;
        if (this.bgTex) gl.deleteTexture(this.bgTex);
        this.bgTex = image ? texture(gl, image) : null;
    };
    Renderer.prototype._target = function (w, h) {
        const gl = this.gl;
        if (w > this.maxSide || h > this.maxSide) throw new Error(`${w}x${h} is past this browser's largest target side (${this.maxSide})`);
        if (this.target && this.target.w === w && this.target.h === h) return this.target;
        if (this.target) { gl.deleteFramebuffer(this.target.fb); gl.deleteTexture(this.target.tex); }
        const tex = texture(gl, { w, h }, gl.NEAREST);
        const fb = gl.createFramebuffer();
        gl.bindFramebuffer(gl.FRAMEBUFFER, fb);
        gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, tex, 0);
        const st = gl.checkFramebufferStatus(gl.FRAMEBUFFER);
        if (st !== gl.FRAMEBUFFER_COMPLETE) throw new Error('the replay target is not complete (' + st + ')');
        this.target = { fb, tex, w, h };
        return this.target;
    };
    Renderer.prototype._quad = function (tex, flip) {
        const gl = this.gl;
        gl.useProgram(this.quadProg);
        gl.activeTexture(gl.TEXTURE0);
        gl.bindTexture(gl.TEXTURE_2D, tex);
        gl.uniform1i(gl.getUniformLocation(this.quadProg, 'u_img'), 0);
        gl.uniform1f(gl.getUniformLocation(this.quadProg, 'u_flip'), flip ? 1 : 0);
        const ap = gl.getAttribLocation(this.quadProg, 'a_p');
        gl.bindBuffer(gl.ARRAY_BUFFER, this.buf.quad);
        gl.enableVertexAttribArray(ap);
        gl.vertexAttribPointer(ap, 2, gl.FLOAT, false, 0, 0);
        gl.drawArrays(gl.TRIANGLES, 0, 6);
        gl.disableVertexAttribArray(ap);
    };

    /* Draw a list into a target of width x height. opts: {background:[r,g,b] 0..255, stretch:bool, enabled:{passes,layers}}.
       Returns {calls, missing:{name:triangles}, missingTris}. */
    Renderer.prototype.draw = function (list, width, height, opts) {
        opts = opts || {};
        const gl = this.gl, L = this.loc;
        const t = this._target(width, height);
        gl.bindFramebuffer(gl.FRAMEBUFFER, t.fb);
        gl.viewport(0, 0, width, height);
        gl.disable(gl.DEPTH_TEST);
        gl.disable(gl.CULL_FACE);
        gl.disable(gl.SCISSOR_TEST);
        gl.disable(gl.BLEND);
        gl.colorMask(true, true, true, true);
        const bg = opts.background || [0, 0, 0];
        gl.clearColor(bg[0] / 255, bg[1] / 255, bg[2] / 255, 0);
        gl.clear(gl.COLOR_BUFFER_BIT);
        if (this.bgTex) this._quad(this.bgTex, false);
        gl.useProgram(this.prog);
        gl.enable(gl.BLEND);
        gl.blendEquation(gl.FUNC_ADD);
        gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);
        gl.uniform2f(L.u_list, list.size[0], list.size[1]);
        gl.uniform2f(L.u_target, width, height);
        gl.uniform1f(L.u_stretch, opts.stretch ? 1 : 0);
        gl.activeTexture(gl.TEXTURE0);
        gl.bindTexture(gl.TEXTURE_2D, this.atlasTex);
        gl.uniform1i(L.u_atlas, 0);
        gl.uniform1f(L.u_hasAtlas, this.atlasTex ? 1 : 0);
        const calls = FLDL.drawOrder(list, opts.enabled);
        const missing = {};
        let missingTris = 0;
        for (const c of calls) {
            const b = FLDL.layerBuffers(list, c.pass, c.index, this.atlas);
            for (const [k, n] of Object.entries(b.missing)) missing[k] = (missing[k] || 0) + n;
            missingTris += b.missingTris;
            this._attr(this.buf.pos, b.pos, L.a_pos, 2, gl.FLOAT, false);
            this._attr(this.buf.col, b.col, L.a_col, 4, gl.UNSIGNED_BYTE, true);
            this._attr(this.buf.uv, b.uv, L.a_uv, 2, gl.FLOAT, false);
            this._attr(this.buf.slot, b.slot, L.a_slot, 1, gl.FLOAT, false);
            gl.uniform1f(L.u_icon, c.op === 'icon' ? 1 : 0);
            gl.colorMask(true, true, true, c.writeAlpha);
            gl.drawArrays(gl.TRIANGLES, 0, b.vertices);
        }
        for (const l of [L.a_pos, L.a_col, L.a_uv, L.a_slot]) gl.disableVertexAttribArray(l);
        gl.colorMask(true, true, true, true);
        gl.disable(gl.BLEND);
        // onto the canvas, 1:1 and flipped for display
        if (this.canvas.width !== width) this.canvas.width = width;
        if (this.canvas.height !== height) this.canvas.height = height;
        gl.bindFramebuffer(gl.FRAMEBUFFER, null);
        gl.viewport(0, 0, width, height);
        this._quad(t.tex, true);
        return { calls, missing, missingTris };
    };
    Renderer.prototype._attr = function (buf, data, loc, size, type, norm) {
        const gl = this.gl;
        gl.bindBuffer(gl.ARRAY_BUFFER, buf);
        gl.bufferData(gl.ARRAY_BUFFER, data, gl.STREAM_DRAW);
        gl.enableVertexAttribArray(loc);
        gl.vertexAttribPointer(loc, size, type, norm, 0, 0);
    };
    /* The replay target's pixels, RGBA with its alpha, row 0 = the list's top row */
    Renderer.prototype.pixels = function () {
        const gl = this.gl, t = this.target;
        if (!t) return new Uint8Array(0);
        const out = new Uint8Array(t.w * t.h * 4);
        gl.bindFramebuffer(gl.FRAMEBUFFER, t.fb);
        gl.readPixels(0, 0, t.w, t.h, gl.RGBA, gl.UNSIGNED_BYTE, out);
        gl.bindFramebuffer(gl.FRAMEBUFFER, null);
        return out;
    };
    /* One pixel of the replay target (list orientation), RGBA */
    Renderer.prototype.pixelAt = function (x, y) {
        const gl = this.gl, t = this.target, out = new Uint8Array(4);
        if (!t || x < 0 || y < 0 || x >= t.w || y >= t.h) return out;
        gl.bindFramebuffer(gl.FRAMEBUFFER, t.fb);
        gl.readPixels(x, y, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, out);
        gl.bindFramebuffer(gl.FRAMEBUFFER, null);
        return out;
    };
    Renderer.prototype.info = function () {
        const gl = this.gl, dbg = gl.getExtension('WEBGL_debug_renderer_info');
        return dbg ? gl.getParameter(dbg.UNMASKED_RENDERER_WEBGL) : gl.getParameter(gl.RENDERER);
    };

    root.FLGL = { Renderer };
})(typeof self !== 'undefined' ? self : this);
