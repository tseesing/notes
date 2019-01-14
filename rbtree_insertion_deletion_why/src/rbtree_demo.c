#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    BLACK = 0,
    RED   = 1
} RBCOLOR;

typedef struct rbnode_s {
    int data;
    RBCOLOR color; 
    struct rbnode_s * left;
    struct rbnode_s * right;
    struct rbnode_s * parent;
} rbnode_t;

typedef struct {
    rbnode_t * root;
    rbnode_t * nil;
} rbtree_t;


rbnode_t * rbtree_successor(rbtree_t * tree, rbnode_t * node)
{
    rbnode_t * p;
    if (node == tree->nil)
        return tree->nil;

    if (node->right != tree->nil)
    {
        p = node->right;
        while (p->left != tree->nil)
            p = p->left;
        return p;
    }
    p = node->parent;
    while (p != tree->nil && node != p->left)
    {
        node = p;
        p = p->parent;
    }
    return p;
}

void rbtree_transplant(rbtree_t * tree, rbnode_t * beplaced, rbnode_t * placing)
{
    if (beplaced == tree->nil)
    {
        return ;
    }
    if (beplaced->parent == tree->nil) // tree->root == beplaced
    {
        tree->root = placing;
    }
    else
    {
        if (beplaced == beplaced->parent->left)
            beplaced->parent->left = placing;
        else
            beplaced->parent->right = placing;

    }
    if (placing != tree->nil)
        placing->parent = beplaced->parent;
}

void rbtree_left_rotate(rbtree_t * tree, rbnode_t * node)
{
    rbnode_t * y = node->right;
    if (y == tree->nil)
        return ;
    y->parent = node->parent;
    if (y->parent != tree->nil)
    {
        if (node == node->parent->left)
            y->parent->left = y;
        else
            y->parent->right = y;
    }
    else
    {
        tree->root = y;
    }
    node->parent = y;
    node->right = y->left;
    if (node->right != tree->nil)
        node->right->parent = node;
    y->left = node;
}

void rbtree_right_rotate(rbtree_t * tree, rbnode_t * node)
{
    rbnode_t * y = node->left;

    if (y == tree->nil)
        return ;
    y->parent = node->parent;
    if (y->parent != tree->nil)
    {
        if (node == node->parent->left)
            y->parent->left = y;
        else
            y->parent->right = y;
    }
    else
    {
        tree->root = y;
    }
    node->parent = y;
    node->left = y->right;
    if (node->left != tree->nil)
        node->left->parent = node;
    y->right = node;
}

void rbtree_insert_fixup (rbtree_t * tree, rbnode_t * node)
{
    rbnode_t * uncle ;
    while (node != tree->nil && node->parent->color == RED)
    {
        if (node->parent == node->parent->parent->left)
        {
            uncle = node->parent->parent->right;
            if (uncle->color == RED)
            { // 父, 子，叔三者同红
                node->parent->parent->color = RED; // 爷节点原来肯定是黑的
                uncle->color = BLACK;
                node->parent->color = BLACK; // 恢复性质4
                node = node->parent->parent; // 爷变红可，可能又与祖同红，同样的问题，重复 while 循环
            }
            else
            { // 叔是黑的 // 如果叔也不是 NIL， 这种情况下，除非是第2次及之后的 while 循环, 从上边的 if 的转入的情况，否则
              // 不可能出现。因为父、子同红，没理由叔子叔是黑的，这样叔子树的黑高会多1.
              // 原来树是平衡的， 现在只有 父子同红的问题，
                if (node == node->parent->right)
                {
                    // 要将 node, node->parent 拉到同一条斜线上，方便接下来的旋转
                    node = node->parent;
                    rbtree_left_rotate(tree, node);
                }
                node->parent->parent->color = RED; // 父是红的，则爷原来只允许是黑的
                node->parent->color = BLACK;
                rbtree_right_rotate(tree, node->parent->parent); // 现在解决了父子同红，其它性质未变，结束.
                /* node = tree->nil; // 照应下循环退出时 node->color = BLACK 的设置 */
            }
        }
        else // node->parent == node->parent->parent->right
        { 
            uncle = node->parent->parent->left;
            if (uncle->color == RED)
            {
                uncle->color = BLACK;
                node->parent->color = BLACK;
                node->parent->parent->color = RED;
                node = node->parent->parent;
            }
            else
            {
                if (node == node->parent->left)
                {
                    node = node->parent;
                    rbtree_right_rotate(tree, node);
                }
                node->parent->color = BLACK;
                node->parent->parent->color = RED;
                rbtree_left_rotate(tree, node->parent->parent);
                /* node = tree->nil; //  */
            }
        }
    }
    // node->color = BLACK; // 如果上边的循环一路搞到了树根，确保让树根变黑吧。
    tree->root->color = BLACK;
}

void rbtree_insert (rbtree_t * tree, rbnode_t * node)
{
    rbnode_t * x, *y;
    node->left  = tree->nil;
    node->right = tree->nil;

    if (tree->root == tree->nil)
    {
        tree->root = node;
        node->color = BLACK;
        node->parent = tree->nil;
        return ;
    }
    x = tree->root;
    while (x != tree->nil)
    {
        y = x;
        if (x->data < node->data)
            x = x->right;
        else
            x = x->left;
    }
    if (y->data < node->data)
        y->right = node;
    else
        y->left = node;
    node->parent = y;
    node->color = RED;
    rbtree_insert_fixup(tree, node);
}

void rbtree_delete_fixup (rbtree_t * tree, rbnode_t * node)
{
    // 目前的问题：node 子树少一个黑高
    rbnode_t * brother;
    while (node != tree->nil && node->color == BLACK)
    {
        if (node == node->parent->left)
        {
            brother = node->parent->right;
            if (brother->color == RED) // 兄弟不管是红是黑，它肯定还有非 nil 的子节点（原来是平衡的, 父左右黑高得一致）
            {// 意味着 node->parent 是黑的, 侄节点是黑的
                brother->color = BLACK;
                rbtree_left_rotate(tree, node->parent); // 重新平衡了， 结束
            }
            else  // borther->color == BLACK
            { // 这里 parent 颜色不明， 看看侄有没有红节点
                if (brother->left->color == BLACK && brother->right->color == BLACK)
                {
                    brother->color = RED; // 实在没办法了。
                    node = node->parent;  // 让上级处理(如果 parent 是黑色，while 继续循环，
                                          // 是红色，while 结束，循环外最后被置黑，重新平衡)
                }
                else
                {
                    if (brother->left->color == RED)
                    {
                        brother->color = RED;
                        brother->left->color = BLACK;
                        rbtree_right_rotate(tree, brother);
                        brother = node->parent->right; // 使 brother 保证是 node 的兄弟
                    }
                    brother->color = node->parent->color;
                    brother->right->color = BLACK;
                    node->parent->color = BLACK; // 重新平衡（node子树补回一个黑高）。
                    rbtree_left_rotate(tree, node->parent);
                }
            } // 总结：兄弟结点是不是红的？是红的，将它变黑，旋转上去作新的父亲，原来的左子树得补回一个黑高。
                // 如果兄弟不是红的，兄弟子树的孩子有没有红的？
                // 若是有红色的，就要想法将该节点通过旋转来顶替兄弟，兄弟则顶替父？父就可以补回 node 子树上
                // 缺失的黑高了。（这里就是判断侄节点有没有红的原因），
                //
                //如果不能。那就没办法了，索性兄弟直接变红吧，造成兄弟的子树也少一个黑高，使得父节点的子树左右先平衡吧。
                //然后，将父节点作为新起点，重复循环，以向上级寻求解法。
                //
        }
        else // (node == node->parent->right)
        {
            brother = node->parent->left;
            if (brother->color == BLACK)
            {
                if (brother->left->color == BLACK && brother->right->color == BLACK)
                {
                    brother->color = RED;
                    node = node->parent;
                }
                else
                {
                    if (brother->right->color == RED)
                    {
                        brother->color = RED;
                        brother->right->color = BLACK;
                        rbtree_left_rotate(tree, brother);
                        brother = node->parent->left;
                    }
                    brother->color = node->parent->color;
                    brother->left->color = BLACK;
                    node->parent->color = BLACK;
                    rbtree_right_rotate(tree, node->parent);
                }
            }
            else
            {
                brother->color = BLACK;
                rbtree_right_rotate(tree, node->parent);
            }
        }
    }
    node->color = BLACK;
}

void rbtree_delete (rbtree_t * tree, rbnode_t * node)
{
    rbnode_t * y, * successor;
    RBCOLOR y_original_color;
    if (node == tree->nil)
        return ;
    if (node->left != tree->nil && node->right != tree->nil)
    { // 后继在其右子树中
        successor = rbtree_successor(tree, node);
        y_original_color = successor->color;
        y = successor->right;
        rbtree_transplant(tree, successor, y);
        successor->right = tree->nil;
        rbtree_transplant(tree, node, successor);
        successor->color = node->color;
        successor->left  = node->left;
        if (node->right != successor)
            successor->right = node->right;
        node->left = tree->nil;
        node->right = tree->nil;
    }
    else
    {
        y = node->left;
        if (node->left == tree->nil)
        {
            y = node->right;
        }
        y_original_color = node->color;
        rbtree_transplant(tree, node, y);
        node->left = tree->nil;
        node->right = tree->nil; // 不执行这两行应该也没问题
    }
    if (y_original_color == BLACK)
        rbtree_delete_fixup(tree, y);
}

void rbtree_preorder(rbtree_t * tree, rbnode_t * node)
{
    if (node != tree->nil)
    {
        printf ("%d(%c)  ", node->data, (node->color == RED ? 'R' : 'B'));
        rbtree_preorder(tree, node->left);
        rbtree_preorder(tree, node->right);
    }
}

void rbtree_inorder(rbtree_t * tree, rbnode_t * node)
{
    if (node != tree->nil)
    {
        rbtree_inorder(tree, node->left);
        printf ("%d(%c)  ", node->data, (node->color == RED ? 'R' : 'B'));
        rbtree_inorder(tree, node->right);
    }
}

void rbtree_postorder(rbtree_t * tree, rbnode_t * node)
{
    if (node != tree->nil)
    {
        rbtree_postorder(tree, node->left);
        rbtree_postorder(tree, node->right);
        printf ("%d(%c)  ", node->data, (node->color == RED ? 'R' : 'B'));
    }
}

void show(rbtree_t *tree)
{
    printf("preorder: \n");
    rbtree_preorder(tree, tree->root);
    putchar('\n');
    printf("inorder: \n");
    rbtree_inorder(tree, tree->root);
    putchar('\n');
    printf("postorder: \n");
    rbtree_postorder(tree, tree->root);
    putchar('\n');
}

rbnode_t * rbtree_find(rbtree_t * tree, int data)
{
    rbnode_t * node ;
    node = tree->root;
    while (node != tree->nil)
    {
        if (node->data > data)
            node = node->left;
        else if (node->data < data)
            node = node->right;
        else
            break;
    }
    return node;
}

// ./a.out 100 3 28 8 23 88 69 89 87
int main(int argc, char **argv)
{
    int i;
    void * mem;
    rbtree_t tree;
    rbnode_t * node;
    rbnode_t nil;
    memset(&tree, 0, sizeof(tree));
    memset(&nil, 0, sizeof(nil));
    nil.color = BLACK;
    tree.nil = &nil;
    tree.root = tree.nil;
    
    mem = calloc(sizeof(rbnode_t), argc - 1);

    node = (rbnode_t* )mem;
    for (i = 1; i < argc; i++)
    {
        node->data = atoi(argv[i]);
        rbtree_insert(&tree, node);
        node++;
    }
    show(&tree);

    node = rbtree_find(&tree, 3);
    rbtree_delete(&tree, node);
    printf("\n\nafter delete. %d(%c) \n", node->data, node->color == RED ? 'R' : 'B');
    show(&tree);

    if (mem)
        free(mem);

    return 0;
}



