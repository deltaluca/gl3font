package gl3font;

import ogl.GL;
import ogl.GLM;
import ogl.GLArray;
import gl3font.GLString;
import goodies.Lazy;
import goodies.Maybe;
import gl3font.PNG;

class Metric {
    public var advance:Float;
    public var left:Float;
    public var top:Float;
    public var width:Float;
    public var height:Float;
    public var u:Float;
    public var v:Float;
    public var w:Float;
    public var h:Float;
    public function new() {}
}
class FontInfo {
    public var idmap:Map<Int,Int>; // map glyph to id

    public var metrics:Array<Metric>;
    public var kerning:Array<Array<Float>>;

    public var height:Float;
    public var ascender:Float;
    public var descender:Float;

    public function new(file:String) {
        var f = sys.io.File.read(file, true);
        var N = f.readInt32();
        height    = f.readFloat();
        ascender  = f.readFloat();
        descender = f.readFloat();
        idmap = new Map<Int,Int>();
        metrics = [for (i in 0...N) {
            idmap.set(f.readInt32(), i);
            var m = new Metric();
            m.advance = f.readFloat();
            m.left    = f.readFloat();
            m.top     = f.readFloat();
            m.width   = f.readFloat();
            m.height  = f.readFloat();
            m.u       = f.readFloat();
            m.v       = f.readFloat();
            m.w       = f.readFloat();
            m.h       = f.readFloat();
            m;
        }];
        var y = 0; var x = 0;
        var kern = []; kerning = [kern];
        while (x < N && y < N) {
            var k = f.readFloat();
            for (i in 0...f.readInt32()) {
                kern[x++] = k;
                if (x == N) {
                    x = 0;
                    kerning.push(kern = []);
                    y++;
                }
            }
        }
    }
}

enum FontAlign {
    AlignLeft;
    AlignRight;
    AlignCentre;
    AlignLeftJustified;
    AlignRightJustified;
    AlignCentreJustified;
}

typedef LineLayout = {
    bounds:Vec2,
    chars:Array<Vec2>
}
typedef TextLayout = {
    bounds:Vec4,
    lines:Array<LineLayout>
};

@:allow(gl3font)
class StringBuffer implements LazyEnv implements MaybeEnv {
    public static inline var VERTEX_SIZE = 8;

    @:lazyVar public var font:Font;
    var pen:Float;
    var staticDraw:Bool;

    public var colours:Array<Vec4>;
    public var vertexData:GLfloatArray;
    public var vertexArray:Int;
    public var vertexBuffer:Int;
    public var numVertices:Int = 0;
    public var invalidated:Bool;
    public inline function reserve(numVerts:Int) {
        invalidated = true;
        var current = numVertices * VERTEX_SIZE;
        if (!staticDraw) {
            var newsize = current + (numVerts * VERTEX_SIZE);
            if (newsize > vertexData.count) {
                var size = vertexData.count;
                do size *= 2 while (size < newsize);
                vertexData.resize(size);
                GL.bindBuffer(GL.ARRAY_BUFFER, vertexBuffer);
                GL.bufferData(GL.ARRAY_BUFFER, vertexData, GL.DYNAMIC_DRAW);
            }
        }
        numVertices += numVerts;
        return current;
    }

    public inline function clear() {
        numVertices = 0;
        invalidated = true;
    }
    public inline function vertex(i:Int, x:Float, y:Float, u:Float, v:Float, col:Vec4) {
        vertexData.subData([x,y,u,v],i);
        vertexData.subDataVec(col, i+4);
    }

    public function new(font:Maybe<Font>, ?size:Maybe<Int>, staticDraw=false) {
        invalidated = true;
        this.staticDraw = staticDraw;
        if (font != null) this.font = font.extract();
        var size = size.or(1); if (size < 1) size = 1;
        vertexData = GL.allocBuffer(GL.FLOAT, VERTEX_SIZE*6*size);
        vertexArray = GL.genVertexArrays(1)[0];
        GL.bindVertexArray(vertexArray);

        vertexBuffer = GL.genBuffers(1)[0];
        GL.bindBuffer(GL.ARRAY_BUFFER, vertexBuffer);
        GL.bufferData(GL.ARRAY_BUFFER, vertexData, staticDraw ? GL.STATIC_DRAW : GL.DYNAMIC_DRAW);
        colours = [];
    }

    public function destroy() {
        GL.deleteVertexArrays([vertexArray]);
        GL.deleteBuffers([vertexBuffer]);
    }

    public static function getLines(string:GLString):Array<GLString> {
        var lines = [];
        var ind;
        while ((ind = string.indexOf("\n")) != -1) {
            lines.push(string.substr(0, ind));
            string = string.substr(ind+1);
        }
        lines.push(string);
        return lines;
    }
    public function set(string:GLString, ?align:Maybe<FontAlign>, ?spacing:Maybe<Float>, computeLayout:Bool=false):Maybe<TextLayout> {
        if (font.info == null) throw "Font has no metrics info";
        var info = font.info.extract();

        var spacing = if (spacing == null) info.height else spacing.extract();

        var layout:Maybe<TextLayout>;
        layout = if (computeLayout) { bounds: new Vec4([1e100,1e100,-1e100,-1e100]), lines: [] } else null;

        clear();

        if (string.length == 0) {
            if (computeLayout) {
                var bounds = layout.extract().bounds;
                bounds.x = bounds.z = 0;
                bounds.y = - info.ascender;
                bounds.w = info.descender + info.ascender;
            }
            return layout;
        }

        var lines = getLines(string);
        var charCount = 0;
        for (l in lines) charCount += l.length;
        var index = reserve(6*charCount);
        var align = align.or(AlignLeft);

        var metrics = [];
        for (line in lines) {
            var minx = 1e100;
            var maxx = -1e100;

            var prev_index:Int = -1;
            var fst = true;
            var pen = 0.0;
            line.iter(function (char:Code, _) {
                var glyph = info.idmap[char];
                var metric = info.metrics[glyph];
                if (!fst) pen += info.kerning[prev_index][glyph];
                fst = false;

                var x0 = metric.left + pen;
                var x1 = x0 + metric.width;

                pen += metric.advance;
                prev_index = glyph;

                if (x0 < minx) minx = x0;
                if (x1 > maxx) maxx = x1;
            });
            metrics.push([minx, maxx-minx]);
        }

        var xform:Array<Array<Float>> = switch (align) {
            case AlignLeft:
                [for (m in metrics) [0,1]];
            case AlignRight:
                [for (m in metrics) [-m[1],1]];
            case AlignCentre:
                [for (m in metrics) [-m[1]/2,1]];
            case AlignLeftJustified:
                var max = 0.0;
                for (m in metrics) if (m[1] > max) max = m[1];
                [for (m in metrics) [0,max/m[1]]];
            case AlignRightJustified:
                var max = 0.0;
                for (m in metrics) if (m[1] > max) max = m[1];
                [for (m in metrics) [-max,max/m[1]]];
            case AlignCentreJustified:
                var max = 0.0;
                for (m in metrics) if (m[1] > max) max = m[1];
                [for (m in metrics) [-max/2,max/m[1]]];
        };

        var data = [];

        var peny = 0.0;
        var i = -1;
        for (line in lines) {
            i++;
            var pen = xform[i][0];
            var scale = xform[i][1];
            var fst = true;
            var prev_index = -1;

            var lineLayout:Maybe<LineLayout>;
            lineLayout = if (computeLayout) { bounds:new Vec2([1e100,-1e100]), chars: [] } else null;

            line.iter(function (char:Code, col:Maybe<Vec4>) {
                var glyph = info.idmap[char];
                var metric = info.metrics[glyph];
                if (!fst) pen += info.kerning[prev_index][glyph]*scale;
                fst = false;

                var x0 = metric.left + pen;
                var y1 = metric.top + peny;
                var x1 = x0 + metric.width;
                var y0 = y1 - metric.height;

                var u0 = metric.u;
                var v0 = metric.v;
                var u1 = u0 + metric.w;
                var v1 = v0 + metric.h;

                #if debug
                    if (col == null) throw 'Character "${String.fromCharCode(char)}" has no colour, rendering line=${line.toString()} of string ${string.toString()}';
                #end
                var col = col.extract();
                var cr = col.r;
                var cg = col.g;
                var cb = col.b;
                var ca = col.a;

                data.push(x0); data.push(y1); data.push(u0); data.push(v1); data.push(cr); data.push(cg); data.push(cb); data.push(ca);
                data.push(x1); data.push(y1); data.push(u1); data.push(v1); data.push(cr); data.push(cg); data.push(cb); data.push(ca);
                data.push(x1); data.push(y0); data.push(u1); data.push(v0); data.push(cr); data.push(cg); data.push(cb); data.push(ca);

                data.push(x0); data.push(y1); data.push(u0); data.push(v1); data.push(cr); data.push(cg); data.push(cb); data.push(ca);
                data.push(x1); data.push(y0); data.push(u1); data.push(v0); data.push(cr); data.push(cg); data.push(cb); data.push(ca);
                data.push(x0); data.push(y0); data.push(u0); data.push(v0); data.push(cr); data.push(cg); data.push(cb); data.push(ca);

                pen += metric.advance*scale;
                prev_index = glyph;

                if (computeLayout) {
                    y1 = peny + info.descender;
                    y0 = peny - info.ascender;
                    var bounds = layout.extract().bounds;
                    if (x0 < bounds.x) bounds.x = x0;
                    if (x1 > bounds.z) bounds.z = x1;

                    var bounds = lineLayout.extract().bounds;
                    if (x0 < bounds.x) bounds.x = x0;
                    if (x1 > bounds.y) bounds.y = x1;

                    lineLayout.extract().chars.push(new Vec2([x0, x1-x0]));
                }
            });

            if (computeLayout) {
                var bounds = lineLayout.extract().bounds;
                if (lineLayout.extract().chars.length != 0)
                     bounds.y -= bounds.x;
                else bounds.x = bounds.y = 0;
                layout.extract().lines.push(lineLayout.extract());
            }

            peny += spacing;
        }

        vertexData.subData(data, 0);

        if (computeLayout) {
            var bounds = layout.extract().bounds;
            if (layout.extract().lines.length != 0) {
                if (bounds.x > bounds.z) bounds.x = bounds.z = 0;
                else bounds.z -= bounds.x;
            }
            else bounds.x = bounds.z = 0;

            bounds.y = -info.ascender;
            bounds.w = (info.ascender - info.descender) + (lines.length - 1) * spacing;
        }

        return layout;
    }
}

class FontRenderer implements LazyEnv implements MaybeEnv {
    var program:GLuint;
    var proj:GLuint;

    public function new() {
        var vShader = GL.createShader(GL.VERTEX_SHADER);
        var fShader = GL.createShader(GL.FRAGMENT_SHADER);
        GL.shaderSource(vShader, "
            #version 130
            in vec2 vPos;
            in vec2 vUV;
            in vec4 vColour;
            uniform mat4 proj;
            out vec2 fUV;
            out vec4 fColour;
            void main() {
                gl_Position = proj*vec4(vPos,0,1);
                fUV = vUV;
                fColour = vColour;
            }
        ");
        GL.shaderSource(fShader, "
            #version 130
            in vec2 fUV;
            in vec4 fColour;
            out vec4 colour;
            uniform sampler2D tex;

            void main() {
                colour = fColour;
                float mask = texture(tex, fUV).r;
                float E = fwidth(mask);
                colour.a *= smoothstep(0.5-E,0.5+E, mask);
            }
        ");
        GL.compileShader(vShader);
        GL.compileShader(fShader);

        program = GL.createProgram();
        GL.attachShader(program, vShader);
        GL.attachShader(program, fShader);

        GL.bindAttribLocation(program, 0, "vPos");
        GL.bindAttribLocation(program, 1, "vUV");
        GL.bindAttribLocation(program, 2, "vColour");
        GL.linkProgram(program);

        GL.deleteShader(vShader);
        GL.deleteShader(fShader);

        proj = GL.getUniformLocation(program, "proj");
    }

    public function destroy() {
        GL.deleteProgram(program);
    }

    public function setTransform(mat:Mat4) {
        GL.uniformMatrix4fv(proj, false, mat);
        return this;
    }

    public function begin() {
        GL.useProgram(program);
        GL.enableVertexAttribArray(0);
        GL.enableVertexAttribArray(1);
        GL.enableVertexAttribArray(2);
        return this;
    }

    public inline function render(str:StringBuffer) {
        renderRaw(str.font.texture, str.vertexBuffer, str.numVertices, str.invalidated ? str.vertexData : null);
        str.invalidated = false;
        return this;
    }

    public function renderRaw(text:GLuint, buffer:GLuint, numVertices:Int, ?vertexData:Maybe<GLfloatArray>=null) {
        GL.bindTexture(GL.TEXTURE_2D, text);
        GL.bindBuffer(GL.ARRAY_BUFFER, buffer);
        GL.vertexAttribPointer(0, 2, GL.FLOAT, false, StringBuffer.VERTEX_SIZE*4, 0*4);
        GL.vertexAttribPointer(1, 2, GL.FLOAT, false, StringBuffer.VERTEX_SIZE*4, 2*4);
        GL.vertexAttribPointer(2, 4, GL.FLOAT, false, StringBuffer.VERTEX_SIZE*4, 4*4);

        if (vertexData != null)
            GL.bufferSubData(GL.ARRAY_BUFFER, 0, GLfloatArray.view(vertexData.extract(), 0, numVertices*StringBuffer.VERTEX_SIZE));
        GL.drawArrays(GL.TRIANGLES, 0, numVertices);
        return this;
    }

    public function end() {
        GL.disableVertexAttribArray(0);
        GL.disableVertexAttribArray(1);
        GL.disableVertexAttribArray(2);
        return this;
    }
}

class Font {
    public var info:Maybe<FontInfo>;
    public var texture:GLuint;
    public function new(datPath:Maybe<String>, dist:String) {
        info = datPath.runOr(function (x) new FontInfo(x), null);
        texture = GL.genTextures(1)[0];
        GL.bindTexture(GL.TEXTURE_2D, texture);

        var png = PNG.grey(dist);
        var bytes:GLubyteArray = new GLubyteArray(cast png.data);
        GL.texImage2D(GL.TEXTURE_2D, 0, GL.LUMINANCE, png.width, png.height, 0, GL.LUMINANCE, bytes);

        GL.texParameteri(GL.TEXTURE_2D, GL.TEXTURE_MAG_FILTER, GL.LINEAR);
        GL.texParameteri(GL.TEXTURE_2D, GL.TEXTURE_MIN_FILTER, GL.LINEAR);
        GL.texParameteri(GL.TEXTURE_2D, GL.TEXTURE_WRAP_S, GL.CLAMP_TO_EDGE);
        GL.texParameteri(GL.TEXTURE_2D, GL.TEXTURE_WRAP_T, GL.CLAMP_TO_EDGE);
    }

    public function destroy() {
        GL.deleteTextures([texture]);
    }
}
