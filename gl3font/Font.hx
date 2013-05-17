package gl3font;

import ogl.GL;
import ogl.GLM;
import ogl.GLArray;

typedef TextBounds = {
    x:Float,
    y:Float,
    w:Float,
    h:Float
};

@:allow(gl3font)
class StringBuffer {
    public static inline var VERTEX_SIZE = 4;

    var font:Font;
    var pen:Float;

    var vertexData:GLfloatArray;
    var vertexArray:Int;
    var vertexBuffer:Int;
    var numVertices:Int = 0;
    inline function reserve(numVerts:Int) {
        var current = numVertices * VERTEX_SIZE;
        var newsize = current + (numVerts * VERTEX_SIZE);
        if (newsize > vertexData.count) {
            var size = vertexData.count;
            do size *= 2 while (size < newsize);
            vertexData.resize(size);
            GL.bindBuffer(GL.ARRAY_BUFFER, vertexBuffer);
            GL.bufferData(GL.ARRAY_BUFFER, vertexData, GL.DYNAMIC_DRAW);
        }
        numVertices += numVerts;
        return current;
    }
    inline function clear() {
        numVertices = 0;
        pen = 0;
    }
    inline function vertex(i:Int, x:Float, y:Float, u:Float, v:Float) {
        vertexData[i+0] = x;
        vertexData[i+1] = y;
        vertexData[i+2] = u;
        vertexData[i+3] = v;
    }

    public function new(font:Font, ?size:Null<Int>, staticDraw=false) {
        this.font = font;
        if (size == null) size = 1;
        vertexData = GL.allocBuffer(GL.FLOAT, VERTEX_SIZE*6*size);
        vertexArray = GL.genVertexArrays(1)[0];
        GL.bindVertexArray(vertexArray);

        vertexBuffer = GL.genBuffers(1)[0];
        GL.bindBuffer(GL.ARRAY_BUFFER, vertexBuffer);
        GL.bufferData(GL.ARRAY_BUFFER, vertexData, staticDraw ? GL.STATIC_DRAW : GL.DYNAMIC_DRAW);
    }

    public function set(string:String):TextBounds {
        clear();
        var size:Int = font.json.pxheight;
        var prev_index:Int = -1;

        var index = reserve(6*string.length);

        var minx = 1e100;
        var miny = 1e100;
        var maxx = -1e100;
        var maxy = -1e100;
        for (i in 0...string.length) {
            var glyph:Int = Reflect.field(font.json.idmap, string.charAt(i));
            var uvwh:{u:Float,v:Float,w:Float,h:Float,sizeu:Float,sizev:Float} = font.json.uvwh[glyph];
            if (i != 0) pen += font.json.kerning[prev_index][glyph];

            var x0 = font.json.offsets[glyph].left + pen;
            var y1 = font.json.offsets[glyph].top;
            var x1 = x0 + uvwh.sizeu;
            var y0 = y1 - uvwh.sizev;

            var u0 = uvwh.u;
            var v0 = uvwh.v;
            var u1 = u0 + uvwh.w;
            var v1 = v0 + uvwh.h;

            vertex(index, x0,y1, u0,v1); index += VERTEX_SIZE;
            vertex(index, x1,y1, u1,v1); index += VERTEX_SIZE;
            vertex(index, x1,y0, u1,v0); index += VERTEX_SIZE;

            vertex(index, x0,y1, u0,v1); index += VERTEX_SIZE;
            vertex(index, x1,y0, u1,v0); index += VERTEX_SIZE;
            vertex(index, x0,y0, u0,v0); index += VERTEX_SIZE;

            pen += font.json.offsets[glyph].advance;
            prev_index = glyph;

            if (x0 < minx) minx = x0;
            if (x1 > maxx) maxx = x1;
            if (y0 < miny) miny = y0;
            if (y1 > maxy) maxy = y1;
        }

        return {
            x : minx,
            y : miny,
            w : maxx-minx,
            h : maxy-miny
        };
    }
}

class FontRenderer {
    var program:GLuint;
    var proj:GLuint;
    var colour:GLuint;

    public function new() {
        var vShader = GL.createShader(GL.VERTEX_SHADER);
        var fShader = GL.createShader(GL.FRAGMENT_SHADER);
        GL.shaderSource(vShader, "
            #version 330 core
            layout(location = 0) in vec2 vPos;
            layout(location = 1) in vec2 vUV;
            uniform mat4 proj;
            out vec2 fUV;
            void main() {
                gl_Position = proj*vec4(vPos,0,1);
                fUV = vUV;
            }
        ");
        GL.shaderSource(fShader, "
            #version 330 core
            in vec2 fUV;
            out vec4 colour;
            uniform sampler2D tex;
            uniform vec4 fontColour;

            void main() {
                float mask = texture(tex, fUV).r;
                float E = fwidth(mask);
                colour = fontColour;
                colour.a = smoothstep(0.5-E,0.5+E, mask);
            }
        ");
        GL.compileShader(vShader);
        GL.compileShader(fShader);

        program = GL.createProgram();
        GL.attachShader(program, vShader);
        GL.attachShader(program, fShader);
        GL.linkProgram(program);
        GL.deleteShader(vShader);
        GL.deleteShader(fShader);

        proj = GL.getUniformLocation(program, "proj");
        colour = GL.getUniformLocation(program, "fontColour");
    }

    public function setTransform(mat:Mat4) {
        GL.uniformMatrix4fv(proj, false, mat);
    }

    public function setColour(colour:Vec4) {
        GL.uniform4fv(this.colour, colour);
    }

    public function begin() {
        GL.useProgram(program);
        GL.enableVertexAttribArray(0);
        GL.enableVertexAttribArray(1);
    }

    public function render(str:StringBuffer) {
        GL.bindTexture(GL.TEXTURE_2D, str.font.texture);
        GL.bindBuffer(GL.ARRAY_BUFFER, str.vertexBuffer);
        GL.vertexAttribPointer(0, 2, GL.FLOAT, false, StringBuffer.VERTEX_SIZE*4, 0);
        GL.vertexAttribPointer(1, 2, GL.FLOAT, false, StringBuffer.VERTEX_SIZE*4, 2*4);

        GL.bufferSubData(GL.ARRAY_BUFFER, 0, GLfloatArray.view(str.vertexData, 0, str.numVertices*StringBuffer.VERTEX_SIZE));
        GL.drawArrays(GL.TRIANGLES, 0, str.numVertices);
    }

    public function end() {
        GL.disableVertexAttribArray(0);
        GL.disableVertexAttribArray(1);
    }
}

@:allow(gl3font)
class Font {
    var json:Dynamic;
    var texture:GLuint;
    public function new(jsonPath:String, dist:String) {
        json = haxe.Json.parse(sys.io.File.getContent(jsonPath));
        texture = GL.genTextures(1)[0];
        GL.bindTexture(GL.TEXTURE_2D, texture);

        var file = sys.io.File.read(dist, true);
        var png = (new format.png.Reader(file)).read();
        var data:GLubyteArray = format.png.Tools.extract32(png).getData();
        var header = format.png.Tools.getHeader(png);
        GL.texImage2D(GL.TEXTURE_2D, 0, GL.LUMINANCE, header.width, header.height, 0, GL.RGBA, data);

        GL.texParameteri(GL.TEXTURE_2D, GL.TEXTURE_MAG_FILTER, GL.LINEAR);
        GL.texParameteri(GL.TEXTURE_2D, GL.TEXTURE_MIN_FILTER, GL.LINEAR);
    }

    public var baseSize(get, never):Float;
    inline function get_baseSize():Float return json.pxheight;
}
