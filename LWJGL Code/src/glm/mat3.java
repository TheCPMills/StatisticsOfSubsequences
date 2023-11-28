package glm;
import java.nio.FloatBuffer;

import org.lwjgl.BufferUtils;

public class mat3 {
    private float m00, m01, m02;
    private float m10, m11, m12;
    private float m20, m21, m22;

    public mat3() {
        m00 = 0f;
        m01 = 0f;
        m02 = 0f;
        m10 = 0f;
        m11 = 0f;
        m12 = 0f;
        m20 = 0f;
        m21 = 0f;
        m22 = 0f;
    }

    public mat3(vec3 column0, vec3 column1, vec3 column2) {
        set(column0, column1, column2);
    }

    public void set(vec3 column0, vec3 column1, vec3 column2) {
        m00 = column0.x;
        m10 = column0.y;
        m20 = column0.z;

        m01 = column1.x;
        m11 = column1.y;
        m21 = column1.z;

        m02 = column2.x;
        m12 = column2.y;
        m22 = column2.z;
    }

    public void set(int columnNumber, vec3 column) {
        switch (columnNumber) {
            case 0:
                m00 = column.x;
                m10 = column.y;
                m20 = column.z;
                break;
            case 1:
                m01 = column.x;
                m11 = column.y;
                m21 = column.z;
                break;
            case 2:
                m02 = column.x;
                m12 = column.y;
                m22 = column.z;
                break;
            default:
                throw new IndexOutOfBoundsException();
        }
    }

    public mat3 add(mat3 other) {
        mat3 result = new mat3();

        result.m00 = this.m00 + other.m00;
        result.m10 = this.m10 + other.m10;
        result.m20 = this.m20 + other.m20;

        result.m01 = this.m01 + other.m01;
        result.m11 = this.m11 + other.m11;
        result.m21 = this.m21 + other.m21;

        result.m02 = this.m02 + other.m02;
        result.m12 = this.m12 + other.m12;
        result.m22 = this.m22 + other.m22;

        return result;
    }

    public mat3 negate() {
        return mult(-1f);
    }

    public mat3 sub(mat3 other) {
        return this.add(other.negate());
    }

    public mat3 mult(float scalar) {
        mat3 result = new mat3();

        result.m00 = this.m00 * scalar;
        result.m10 = this.m10 * scalar;
        result.m20 = this.m20 * scalar;

        result.m01 = this.m01 * scalar;
        result.m11 = this.m11 * scalar;
        result.m21 = this.m21 * scalar;

        result.m02 = this.m02 * scalar;
        result.m12 = this.m12 * scalar;
        result.m22 = this.m22 * scalar;

        return result;
    }

    public vec3 mult(vec3 vector) {
        float x = this.m00 * vector.x + this.m01 * vector.y + this.m02 * vector.z;
        float y = this.m10 * vector.x + this.m11 * vector.y + this.m12 * vector.z;
        float z = this.m20 * vector.x + this.m21 * vector.y + this.m22 * vector.z;
        return new vec3(x, y, z);
    }

    public mat3 mult(mat3 other) {
        mat3 result = new mat3();

        result.m00 = this.m00 * other.m00 + this.m01 * other.m10 + this.m02 * other.m20;
        result.m10 = this.m10 * other.m00 + this.m11 * other.m10 + this.m12 * other.m20;
        result.m20 = this.m20 * other.m00 + this.m21 * other.m10 + this.m22 * other.m20;

        result.m01 = this.m00 * other.m01 + this.m01 * other.m11 + this.m02 * other.m21;
        result.m11 = this.m10 * other.m01 + this.m11 * other.m11 + this.m12 * other.m21;
        result.m21 = this.m20 * other.m01 + this.m21 * other.m11 + this.m22 * other.m21;

        result.m02 = this.m00 * other.m02 + this.m01 * other.m12 + this.m02 * other.m22;
        result.m12 = this.m10 * other.m02 + this.m11 * other.m12 + this.m12 * other.m22;
        result.m22 = this.m20 * other.m02 + this.m21 * other.m12 + this.m22 * other.m22;

        return result;
    }

    public mat3 transpose() {
        mat3 result = new mat3();

        result.m00 = this.m00;
        result.m10 = this.m01;
        result.m20 = this.m02;

        result.m01 = this.m10;
        result.m11 = this.m11;
        result.m21 = this.m12;

        result.m02 = this.m20;
        result.m12 = this.m21;
        result.m22 = this.m22;

        return result;
    }

    public FloatBuffer toBuffer() {
        FloatBuffer buffer = BufferUtils.createFloatBuffer(9);

        buffer.put(m00);
        buffer.put(m10);
        buffer.put(m20);

        buffer.put(m01);
        buffer.put(m11);
        buffer.put(m21);
        
        buffer.put(m02);
        buffer.put(m12);
        buffer.put(m22);

        buffer.flip();
        
        return buffer;
    }

    public static final mat3 identity() {
        mat3 identityMatrix = new mat3();

        identityMatrix.m00 = 1f;
        identityMatrix.m11 = 1f;
        identityMatrix.m22 = 1f;

        return identityMatrix;
    }
}