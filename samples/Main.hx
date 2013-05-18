package;

import ogl.GL;
import ogl.GLM;
import glfw3.GLFW;
import gl3font.Font;

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
        var font = new Font(face+".dat", face+".png");

        var buf = new StringBuffer(font);
        buf.set("µ®½§¥¹»ßðØæÐË¶º\n!!!Hello World!!!", AlignCentreJustified);

        var renderer = new FontRenderer();
        while (!GLFW.windowShouldClose(window)) {
            GLFW.pollEvents();

            var time = GLFW.getTime();
            var rotation = time*0;
            var size = 80 + Math.sin(time*0.5)*70*0-30;
            var pos = GLFW.getCursorPos(window);
            var x = pos.x;
            var y = pos.y;

            GL.clear(GL.COLOR_BUFFER_BIT);

            renderer.begin();
            renderer.setColour([1,1,1,1]);
            renderer.setTransform(
                Mat3x2.viewportMap(800, 600) *
                Mat3x2.translate(x, y) *
                Mat3x2.scale(size, size) *
                Mat3x2.rotate(rotation)
            );
            renderer.render(buf);
            renderer.end();

            GLFW.swapBuffers(window);
        }
    }
}

