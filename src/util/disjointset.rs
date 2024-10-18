use crate::util::graph::IndexType;

pub struct DisjointSet {
    tree : Vec<IndexType>,
}

impl DisjointSet {
    pub fn new(size: usize) -> DisjointSet {
        DisjointSet { tree: Vec::from_iter(0..size) }
    }

    pub fn isroot(&self, v: IndexType) -> bool {
        self.tree[v] == v
    }

    pub fn root(&self, mut v: IndexType) -> IndexType {
        while !self.isroot(v) {
            v = self.tree[v];
        }
        v
    }

    pub fn root_compress(&mut self, v: IndexType) -> IndexType {
        self.compress(v);
        // compressed nodes must satisfy isroot == true
        self.tree[v]
    }

    pub fn link(&mut self, from: IndexType, to: IndexType) {
        let (fr, tr) = (self.root_compress(from), self.root_compress(to));
        self.tree[fr] = self.tree[tr];
    }

    fn compress(&mut self, mut v: IndexType) {
        let r = self.root(v);
        while self.tree[v] != r {
            let nxt = self.tree[v];
            self.tree[v] = r;
            v = nxt;
        }
    }
}
