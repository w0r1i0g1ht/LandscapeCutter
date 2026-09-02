# 旧版 Python 原型

最终 Python 实现保存在带注释的 Git 标签 `python-prototype-final` 中。

可使用以下命令查看内容，而不改变当前分支：

```powershell
git show python-prototype-final:README.md
git ls-tree -r --name-only python-prototype-final
```

如需运行或修改原型，请从该标签创建单独的分支或工作树。当前 `main` 分支只包含
C++ 实现，不维持与 Python 文件布局或 API 的兼容性。
