# Blinn-Phong 模型介绍

Blinn-Phong 模型是一种着色模型。该模型将光照对物体的作用分成三部分
1. 漫反射
2. 高光
3. 环境光

![stage](stage.png)


## 漫反射

$$
diffuse = k_d\cdot (I/r^2) \max(0, \vec{n}\cdot \vec{h})
$$
