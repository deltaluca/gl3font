package;

import ogl.GL;
import ogl.GLM;
import glfw3.GLFW;
import gl3font.Font;
import gl3font.GLString;

class Main {
    static function main() {
        GLFW.init();
        var window = GLFW.createWindow(800, 600, "Test GL3Font");
        GLFW.makeContextCurrent(window);
        GL.init();

        GL.viewport(0,0,800,600);
        GL.enable(GL.BLEND);
        GL.blendFunc(GL.SRC_ALPHA, GL.ONE_MINUS_SRC_ALPHA);

        var face = "../dejavu/serif";
        var font = new Font('$face.dat', '$face.png');

        var buf = new StringBuffer(font);
        var str = GLString.make("µ®½§¥¹»ß", [1,0,0,1]) + GLString.make("ðØæÐË¶º!", [1,1,1,1]);
        buf.set(str, AlignCentre);

        var renderer = new FontRenderer();
        while (!GLFW.windowShouldClose(window)) {
            GLFW.pollEvents();

            var time = GLFW.getTime();
            var rotation = time;
            var size = 160 + Math.sin(time*0.5)*120;
            var pos = GLFW.getCursorPos(window);
            var x = pos.x*0+400;
            var y = pos.y*0+300;

            GL.clear(GL.COLOR_BUFFER_BIT);

            renderer.begin();
            renderer.setTransform(
                Mat3x2.viewportMap(800, 600) *
                Mat3x2.translate(x, y) *
                Mat3x2.scale(size, size) *
                new Mat3x2([1, Math.cos(time), 0, 1, 0, 0]) *
                Mat3x2.rotate(rotation)
            );
            renderer.render(buf);
            renderer.end();

            GLFW.swapBuffers(window);
        }

        buf.destroy();
        font.destroy();
        renderer.destroy();

        GLFW.destroyWindow(window);
        GLFW.terminate();
    }
}

