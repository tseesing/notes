tornado 架构的 UIModule
=======================


| 日期  | 2018-04-08 |
|:------|:-------:|
|作者   | tseesing|

如果觉得有用, 转载自便, 但请保留内容完整, 勿断章取义!


tornado 中的 UIModule, 有些与众不同. 一般的, 是 Handler 中 render 将模板表达式求值或将值替换变量就完事了，是“单向”的过程，而有了 UIModule 后，还可以在模板中再调用外部的自定义的 Python 代码. 


# 功能

顾名思义, 就是 UI 模块. 结合 tornado 模板中 module 命令, 将 UIModule 中
生成的内容作为 module 表达式的结果.  就是在这里, 通过 `module` 来调用 
Python 代码! 这里所说的 **Python 代码** , 要求是 *tornado.web.UIModule* 的子类.


# UIModule 自身

模块位置: tornado.web.UIModule

其实我目前所了解的, 比 help 的多不了多少.  所以, 开发时, 使用以下方法在 Python shell 中尽情地看吧... help 文档还 **权威** 、**更完整** 一些.

```python
[root@localhost uimodule_test]# python3
>>> from tornado.web import UIModule
>>> help(UIModule)
```

尽管如此, 看在内容不多的份上，还是抄录翻译下, 加点自己的见解吧, 希望没误导人....


UIModule 的方法:

```python

def render(self, *args, **kwargs):
    """ 
    此方法在子类中必须重载(override) 
    在模板中执行 module <SubUIModule> 时, 会使用此方法返回本模块的结果。
    返回字符串即可
    args, kwargs 参数都是模板中 module 指令传递过来的 context
    """
    ...

# 如:

def render(self, name):
    """
    模板中遇到指令 {% module <this_module_name>("kitty") %} ，
    该示例返回的结果就是:
    "<h1>Hello: kitty</h1>"
    """
    return "<h1>Hello: " + name + "</h1>"
    

def css_files(self):
    """
    重载此方法，可将本 UI 模块需要的 CSS 文件包含进模板页面中
    如果 css 文件是相对路径，则将加上 RequestHandler.static_url 作为
    路径前缀；
    如果是绝对路径，则原样使用
    """
    ...

#    如：
def css_files(self):
    return ["ui.css", "/foo.css"]
    """
    在 module 所在的模板网页的 <head></head> 插入如下内容
    <link href="/static/ui.css?v=befe55cc43f486d357d890bd19de2a5d" type="text/css" rel="stylesheet"/><link href="/foo.css" type="text/css" rel="stylesheet"/>
    """


def embedded_css(self):
    """
    与 css_files(self) 方法类似，不过返回的是嵌入到页面的样式
    """

# 示例
def embedded_css(self):
    return "#greeting { color: blue; font-weight: bold; }"
    """
    本例子将嵌入如下内容到 <head> 标签中：
    <style type="text/css">
        #greeting { color: blue; font-weight: bold; }
    </style>
    """

def embedded_javascript(self):
    ...

def javascript_files(self):
    ...

    """
    以上两个方法分别与 embedded_css() 及 css_files() 两个方法类似，用法同样
    嵌入的 javascript 及 javascript 文件都紧挨 </body> 标签之前
    """


def html_body(self):
    """
    此方法返回 HTML 字符串，会插入到模板文件 </body> 标签之前
    """

def html_head(self):
    """
    此方法返回 HTML 字符串，会插入到模板文件 </head> 标签之前
    """

def render_string(self, path, **kwargs):
    """
    render 参数 path 指向模板文件。
    如果不明白此方法意义，那肯定是!!!不必!!!重载.

    UIModule 类中该方法已经被实现为 render 一个模板文件，并将 render 后
    的 HTML 字符串返回. path 参数为模板文件， kwargs 参数为传递给模
    板的 context。

    自定义的 UIModule 派生类的方法中，如果需要 render 一个模板，直接调用即可
    """
    ...

# 如

class MyUIModule(UIModule):
    def render(self, *entry):
        return self.render_string("test2.html", theVar = "12345678")

```



# 代码演示

```python
# 首先, 定义 "UI" 模块
from tornado.web import UIModule

class MyUIModule(UIModule):
    """
        必须得继承自 UIModule 
    """
    def render(self, entry):
        # return self.render_string("test2.html", theVar = "12345678")
        return '<span id="greeting">Hello from UIModule</span>'
    
    def css_files(self):
        return ["ui.css", "/foo.css"]

    def javascript_files(self):
        return ["fake.js", "/still_fake.js"]
    
    def embedded_javascript(self):
        return 'var gVar = "global";'

    def embedded_css(self):
        return "#greeting { color: blue; font-weight: bold; }"

    def html_head(self):
        return '<meta name="x"  content="杂乱"/>'

    def html_body(self):
        return "<h2>body text from UIModule</h2>"


# 在定义 Application 实例时，ui_modules 参数指定 UI 模块
tornado.web.Application ([
    (r'/', Handler)
    ],
    # 可以是 dict 类型，dict 中 key 是为模块自定义的名字; value 则是模块类的名字
    # 也可以是模块(独立的文件); 
    # 也可以是 list, 但 list 内的元素需是 dict 类型,如 [{"aui" : MyAUI}, {"bui" : MyBUI}]

    ui_modules = {"MyUI" : MyUIModule}, 
    static_path = os.path.join(os.path.dirname(__file__), "static"),
    template_path = os.path.join(os.path.dirname(__file__), "templates"),
    debug = True,
    )


# 接着, 在模板中使用 module 指令来调用 UI 模块
<!DOCTYPE html>
<html>
  <head>
    <meta charset="UTF-8">
    <title>UIModule 测试</title>
  </head>
  <body>
    <h1>{% module MyUI(msg) %}</h1>
  </body>
</html>

```

之后，在 Handler 中 render 这个模板即可 

以上的代码片断，输出结果如下：
```html
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<title>UIModule 测试</title>
<link href="/static/ui.css?v=befe55cc43f486d357d890bd19de2a5d" type="text/css" rel="stylesheet"/><link href="/foo.css" type="text/css" rel="stylesheet"/>
<style type="text/css">
#greeting { color: blue; font-weight: bold; }
</style>
<meta name="x"  content="杂乱"/>
</head>
<body>
<h1><span id="greeting">Hello from UIModule</span></h1>
<script src="/static/fake.js?v=d41d8cd98f00b204e9800998ecf8427e" type="text/javascript"></script><script src="/still_fake.js" type="text/javascript"></script>
<script type="text/javascript">
//<![CDATA[
var gVar = "global";
//]]>
</script>
<h2>body text from UIModule</h2>
</body>
</html>
```


# 参考

[1] "tornado 文档 (特别是模板部分)" <http://www.tornadoweb.org/en/stable/template.html>

[2] tornado.web.UIModule 的 help 手册



