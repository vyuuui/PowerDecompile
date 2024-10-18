use crate::util::disjointset::DisjointSet;

pub type IndexType = usize;
const INVALID_INDEX: IndexType = usize::MAX;

pub trait Node {
    fn edge_iter(&self, fwd: bool) -> impl Iterator<Item = IndexType>;
}

pub struct PreorderIterator {
    vis: Vec<bool>,
    stack: Vec<(IndexType, IndexType)>,
    fwd: bool,
}

pub struct PostorderIterator {
    vis: Vec<bool>,
    stack: Vec<(IndexType, usize)>,
    path: Vec<IndexType>,
    fwd: bool,
    depth: usize,
}

pub trait Graph<'a, T>
where
    T: Node + 'a,
{
    type VertexType;

    fn root(&self) -> IndexType;
    fn term(&self) -> IndexType;

    fn vert(&self, id: IndexType) -> Option<&'a T>;
    fn vert_mut(&mut self, id: IndexType) -> Option<&'a mut T>;

    fn size(&self) -> usize;

    fn preorder_iter(&self, fwd: bool) -> PreorderIterator {
        PreorderIterator::new(self.root(), self.size(), fwd)
    }

    fn postorder_iter(&self, fwd: bool) -> PostorderIterator {
        PostorderIterator::new(self.root(), self.size(), fwd)
    }

    fn min_vert(
        v: IndexType,
        sdom: &[IndexType],
        parent: &[IndexType],
        forest: &mut DisjointSet,
    ) -> IndexType {
        let r = forest.root_compress(v);
        if r == v {
            return v;
        }

        // Find the vertex "u" with minimal semidominator along r->v
        let (mut min_u, mut u) = (v, parent[v]);
        while u != r {
            if sdom[u] < sdom[min_u] {
                min_u = u;
            }
            u = parent[u];
        }
        min_u
    }

    fn compute_dom_tree(&self, fwd: bool) -> Vec<IndexType> {
        let mut idom: Vec<IndexType> = vec![0; self.size()];

        // Notation and symbols:
        //   G = Control flow graph
        //   T = DFS tree
        //   *_gr -> Graph node number
        //   *_dfs -> DFS tree number

        // Step 1: compute dfs number and tree for all v ∈ G
        // stores DFS->vert in sdom, vert->DFS in dfs2gr
        let mut sdom: Vec<IndexType> = vec![INVALID_INDEX; self.size()];
        let mut dfs2vert: Vec<IndexType> = vec![0; self.size()];
        let mut parent: Vec<IndexType> = vec![0; self.size()];

        let mut dfs_num = 0;
        let mut preorder = self.preorder_iter(fwd);
        while let Some((cv, pv)) = preorder.next(self) {
            sdom[cv] = dfs_num;
            dfs2vert[dfs_num] = cv;
            parent[cv] = pv;
            dfs_num += 1;
        }

        // Step 2: compute semidominator into sdom(v) for v ∈ T
        // Step 3: find immediate dominators
        let mut bucket: Vec<bool> = vec![false; self.size() * self.size()];
        let mut forest = DisjointSet::new(self.size());

        let (mut w_dfs, mut w_gr) = (self.size() - 1, dfs2vert[self.size() - 1]);
        while w_dfs > 0 {
            let w_blk = self.vert(w_gr).unwrap();
            for v_gr in w_blk.edge_iter(fwd) {
                let u_gr = Self::min_vert(v_gr, &sdom, &parent, &mut forest);
                sdom[w_gr] = std::cmp::min(sdom[w_gr], sdom[u_gr]);
            }

            forest.link(w_gr, parent[w_gr]);
            bucket[dfs2vert[sdom[w_gr]] * self.size() + w_gr] = true;

            let bucket_start = parent[w_gr] * self.size();
            // TODO: switch to set/hashset?
            let w_bucket = &mut bucket[bucket_start..bucket_start + self.size()];
            for (v_gr, filled) in w_bucket.iter().enumerate() {
                if !*filled {
                    continue;
                }

                let u_gr = Self::min_vert(v_gr, &sdom, &parent, &mut forest);
                // idom is either known now (parent[w]), or must be deferred (defer to idom[u])
                idom[v_gr] = if sdom[u_gr] < sdom[v_gr] {
                    u_gr
                } else {
                    parent[w_gr]
                }
            }

            w_bucket.fill(false);

            w_dfs -= 1;
            w_gr = dfs2vert[w_dfs];
        }

        (w_dfs, w_gr) = (1, dfs2vert[1]);
        while w_dfs < self.size() {
            if idom[w_gr] != dfs2vert[sdom[w_gr]] {
                idom[w_gr] = idom[idom[w_gr]];
            }
            w_dfs += 1;
            w_gr = dfs2vert[w_dfs];
        }

        if fwd {
            idom[self.root()] = self.root();
        } else {
            idom[self.term()] = self.term();
        }
        idom
    }
}

pub fn dominates(dom_tree: &[IndexType], n: IndexType, m: IndexType) -> bool {
    let mut it = m;
    while dom_tree[it] != it && it != n {
        it = dom_tree[it];
    }

    it == n
}

impl PreorderIterator {
    fn new(start: IndexType, size: usize, fwd: bool) -> Self {
        PreorderIterator {
            vis: vec![false; size],
            stack: vec![(start, start)],
            fwd,
        }
    }

    pub fn next<'a, T, U>(&mut self, graph: &T) -> Option<(IndexType, IndexType)>
    where
        T: Graph<'a, U> + ?Sized,
        U: Node + 'a,
    {
        let (cv, pv) = loop {
            let (cv, pv) = self.stack.pop()?;
            if self.vis[cv] {
                break (cv, pv);
            }
        };

        let vert = graph.vert(cv).unwrap();
        for nv in vert.edge_iter(self.fwd) {
            if !self.vis[nv] {
                self.stack.push((nv, cv));
            }
        }
        Some((cv, pv))
    }
}

impl PostorderIterator {
    fn new(start: IndexType, size: usize, fwd: bool) -> Self {
        PostorderIterator {
            vis: vec![false; size],
            stack: vec![(start, 0)],
            path: Vec::new(),
            fwd,
            depth: 0,
        }
    }

    pub fn next<'a, T, U>(&mut self, graph: T) -> Option<IndexType>
    where
        T: Graph<'a, U>,
        U: Node + 'a,
    {
        // To handle pruning a path switch
        if self.path.len() > self.depth {
            return self.path.pop();
        }

        // Completed traversal pruning is handled implicitly in while condition
        while let Some((cv, depth)) = self.stack.pop() {
            if self.vis[cv] {
                continue;
            }
            self.vis[cv] = true;

            if self.path.len() > self.depth {
                break;
            }

            let vert = graph.vert(cv).unwrap();
            for nv in vert.edge_iter(self.fwd) {
                if !self.vis[nv] {
                    self.stack.push((nv, depth + 1));
                }
            }
        }

        self.path.pop()
    }
}
