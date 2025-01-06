// COMPLETED: Postorder substitution, Graph substitution
// TO BE COMPLETED: Acyclic region detection, cyclic region detection, tail
// region detection, refinement, improved cyclic region detection

use crate::ir::pdir::PDRoutine;
use crate::util::graph::{Graph, IndexType, INVALID_INDEX};
use std::collections::VecDeque;

pub trait Structurizer {
    // TODO: Result type?
    fn structurize(&mut self);
}

pub struct INode {
    tp: NodeType,
    // Index within graph
    gr_idx: IndexType,
}

#[derive(Clone, Copy)]
pub enum CCondRef {
    Leaf(IndexType),
    Compound(IndexType),
}

pub struct CCondData {
    lhs: CCondRef,
    rhs: CCondRef,
    is_and: bool,
    is_inv: bool,
}

pub enum NodeType {
    // Basic block
    Leaf(IndexType),

    /////////////////////
    // Acyclic regions //
    /////////////////////
    // Nodes
    Seq(Vec<IndexType>),
    // Head, True
    If(IndexType, IndexType),
    // Head, False
    IfInv(IndexType, IndexType),
    // Head, True, False
    IfElse(IndexType, IndexType, IndexType),
    // Counter-intuitively, this is an expression and not a structure, a design flaw?
    CompoundCond(Vec<IndexType>, Vec<CCondData>),
    // Head, Cases
    Switch(IndexType, Vec<IndexType>),

    ////////////////////
    // Cyclic regions //
    ////////////////////
    // Body
    SelfLoop(IndexType),
    // Cond, Body
    While(IndexType, IndexType),
    // Body, Cond
    DoWhile(IndexType, IndexType),
    // Init, Cond, Body + Iter
    For(IndexType, IndexType, IndexType),

    /////////////////////////
    // Irreducible regions //
    /////////////////////////
    Goto(IndexType),
    Tail,
    Invalid,
}

struct SPSState {
    reduce_graph: Graph<INode>,
    dom: Vec<IndexType>,
}

impl SPSState {
    fn new(flowgraph: &PDRoutine) -> SPSState {
        // TODO: This is unnecessarily slow
        let mut copy_graph = Graph::default();
        for (idx, v) in flowgraph.blocks.vert_iter().enumerate() {
            if v.is_pseudo() {
                continue;
            }
            let val = INode {
                // TODO: Get source index from node instead here
                tp: NodeType::Leaf(idx),
                gr_idx: copy_graph.size(),
            };
            copy_graph.add_vert(val);
        }
        for (from, v) in flowgraph.blocks.vert_iter().enumerate() {
            for to in v.edge_iter(true) {
                copy_graph.link(from, to);
            }
        }

        SPSState {
            reduce_graph: copy_graph,
            dom: Vec::new(),
        }
    }

    fn add_worknode(&mut self) -> IndexType {
        let temp = INode {
            tp: NodeType::Invalid,
            gr_idx: self.reduce_graph.size(),
        };
        self.reduce_graph.add_vert(temp)
    }
}

struct NodeSet {
    set: Vec<(IndexType, IndexType, IndexType)>,
    head: IndexType,
    tail: IndexType,
    sz: usize,
}

struct NodeSetIter<'a> {
    parent: &'a NodeSet,
    idx: IndexType,
}

impl Iterator for NodeSetIter<'_> {
    type Item = IndexType;

    fn next(&mut self) -> Option<Self::Item> {
        if self.idx == INVALID_INDEX {
            return None;
        }

        let c = self.idx;
        self.idx = self.parent.set[c].2;
        Some(c)
    }
}

impl NodeSet {
    fn new(cap: usize) -> NodeSet {
        NodeSet {
            set: vec![(0, INVALID_INDEX, INVALID_INDEX); cap],
            head: INVALID_INDEX,
            tail: INVALID_INDEX,
            sz: INVALID_INDEX,
        }
    }

    fn iter(&self) -> NodeSetIter {
        NodeSetIter {
            parent: self,
            idx: self.head,
        }
    }

    fn inc(&mut self, idx: usize) {
        if self.set[idx].0 == 0 {
            self.add(idx);
        }
        self.set[idx].0 += 1;
    }

    fn add(&mut self, idx: usize) {
        let e = &mut self.set[idx];
        e.1 = self.tail;
        e.2 = INVALID_INDEX;
        if self.sz == 0 {
            self.tail = idx;
            self.head = self.tail;
        } else {
            self.set[self.tail].2 = idx;
            self.tail = idx;
        }
        self.sz += 1;
    }

    fn erase(&mut self, idx: usize) {
        if self.set[idx].0 == 0 {
            return;
        }

        self.set[idx].0 = 0;
        let (prev, next) = (self.set[idx].1, self.set[idx].2);
        if prev != INVALID_INDEX {
            self.set[prev].2 = next;
        } else {
            self.head = next;
        }

        if next != INVALID_INDEX {
            self.set[next].1 = prev;
        } else {
            self.tail = prev;
        }
        self.sz -= 1;
    }

    fn dec(&mut self, idx: usize) {
        match self.set[idx].0 {
            0 => (),
            1 => self.erase(idx),
            _ => self.set[idx].0 -= 1,
        }
    }
}

// This algorithm is an implementation of the following paper:
// Tao Wei, Jian Mao, Wei Zou, and Yu Chen. 2007. Structuring 2-way Branches in Binary Executables. In Proceedings of
// the 31st Annual International Computer Software and Applications Conference - Volume 01 (COMPSAC '07). IEEE Computer
// Society, USA, 115–118. https://doi.org/10.1109/COMPSAC.2007.203
fn guess_compound_conditional(state: &mut SPSState, idx: IndexType) -> Option<NodeType> {
    let mut pord: Vec<IndexType> = Vec::new();
    let mut in_pord: Vec<bool> = vec![false; state.reduce_graph.size()];
    let mut out_set = NodeSet::new(state.reduce_graph.size());
    let mut pend: VecDeque<IndexType> = VecDeque::new();

    // Expansion Phase
    pord.push(idx);
    in_pord[idx] = true;
    {
        let (a, b) = state.reduce_graph.vert(idx).unwrap().icbs().unwrap();
        pend.push_back(a);
        pend.push_back(b);
        out_set.inc(a);
        out_set.inc(b);
    }

    while let Some(cur_idx) = pend.pop_front() {
        let cur = state.reduce_graph.vert(cur_idx).unwrap();
        if in_pord[cur_idx] {
            continue;
        }

        let Some((l_idx, r_idx)) = cur.icbs() else {
            continue;
        };

        let mut incoming_nodes_in_set = true;
        for i in cur.edge_iter(false) {
            incoming_nodes_in_set &= in_pord[i];
        }
        if !incoming_nodes_in_set {
            continue;
        }

        pord.push(cur_idx);
        in_pord[cur_idx] = true;
        pend.push_back(l_idx);
        pend.push_back(r_idx);
        out_set.erase(cur_idx);
        out_set.inc(l_idx);
        out_set.inc(r_idx);
    }

    // Contraction phase
    while out_set.sz > 2 {
        let removed_idx = pord.pop().unwrap();
        let removed = state.reduce_graph.vert(removed_idx).unwrap();

        let (l_idx, r_idx) = removed.icbs().unwrap();
        out_set.dec(l_idx);
        out_set.dec(r_idx);
        out_set.inc(removed_idx);
    }

    if pord.len() == 1 {
        return None;
    }

    // This is an extension to the above algorithm which will determine
    // the precedence of ANDs and ORs, folding it into a tree
    let mut gr: Graph<CCondRef> = Graph::default();
    let mut result: Vec<CCondData> = Vec::new();

    for v in pord.iter() {
        gr.add_vert(CCondRef::Leaf(*v));
    }
    for v in out_set.iter() {
        gr.add_vert(CCondRef::Leaf(v));
    }
    for v in pord.iter() {
        for t in state.reduce_graph.vert(*v).unwrap().edge_iter(true) {
            gr.link(*v, t);
        }
    }

    // The goal is to reduce this down to the expression and the true/false cases
    while gr.size() > 3 {
        let (mut n0, mut n1, mut is_and, mut is_inv) = (0, 0, false, false);
        for v in gr.vert_iter() {
            let Some((e0, e1)) = v.icbs() else { continue };
            let v0 = gr.vert(e0).unwrap();
            let v1 = gr.vert(e1).unwrap();
            if match (v0.icbs(), v1.icbs()) {
                (Some((e0t, e0f)), Some((_, e1f))) if e0t == e1 => {
                    (n0, n1, is_and, is_inv) = (v.index(), v0.index(), false, e0f == e1f);
                    true
                }
                (Some((e0t, e0f)), Some((e1t, _))) if e0f == e1 => {
                    (n0, n1, is_and, is_inv) = (v.index(), v0.index(), true, e0t == e1t);
                    true
                }
                (Some((_, e0f)), Some((e1t, e1f))) if e1t == e0 => {
                    (n0, n1, is_and, is_inv) = (v.index(), v1.index(), true, e1f == e0f);
                    true
                }
                (Some((e0t, _)), Some((e1t, e1f))) if e1f == e0 => {
                    (n0, n1, is_and, is_inv) = (v.index(), v1.index(), true, e1t == e0t);
                    true
                }
                _ => false,
            } {
                break;
            }
        }

        // TODO: check this one
        let repl = gr.add_vert(CCondRef::Compound(result.len()));
        result.push(CCondData {
            lhs: *gr.vert(n0).unwrap().data().unwrap(),
            rhs: *gr.vert(n1).unwrap().data().unwrap(),
            is_and,
            is_inv,
        });
        gr.cut(n0, n1);
        gr.relink_in_multiple(&[n0, n1], repl);
        gr.relink_out_multiple(&[n0, n1], repl);
        gr.detach(n0);
        gr.detach(n1);
    }

    Some(NodeType::CompoundCond(pord, result))
}

fn acyclic_region_type(state: &mut SPSState, idx: IndexType) -> Option<NodeType> {
    {
        // Sequential regions
        let mut seqlist: Vec<IndexType> = Vec::new();

        let mut iter = state.reduce_graph.vert(idx).unwrap().single_pred();
        while let Some(pred) = iter {
            if state
                .reduce_graph
                .vert(pred)
                .unwrap()
                .single_succ()
                .is_none()
            {
                break;
            }

            seqlist.push(pred);
            iter = state.reduce_graph.vert(pred).unwrap().single_pred();
        }

        seqlist.reverse();
        seqlist.push(idx);

        iter = state.reduce_graph.vert(idx).unwrap().single_succ();
        while let Some(succ) = iter {
            if state
                .reduce_graph
                .vert(succ)
                .unwrap()
                .single_pred()
                .is_none()
            {
                break;
            }
            seqlist.push(succ);
            iter = state.reduce_graph.vert(succ).unwrap().single_succ();
        }

        if seqlist.len() > 1 {
            return Some(NodeType::Seq(seqlist));
        }
    }

    if let Some((m_idx, n_idx)) = state.reduce_graph.vert(idx).unwrap().icbs() {
        let m = state.reduce_graph.vert(m_idx).unwrap();
        let n = state.reduce_graph.vert(n_idx).unwrap();
        return if m.is_spss() && n.is_spss() && m.single_succ().unwrap() == n.single_succ().unwrap()
        {
            // M will always be the true edge for a binary conditional node
            Some(NodeType::IfElse(idx, m_idx, n_idx))
        } else if m.is_spss() && n.incoming_match(&[idx, m_idx]) {
            Some(NodeType::If(idx, m_idx))
        } else if n.is_spss() && m.incoming_match(&[idx, n_idx]) {
            Some(NodeType::IfInv(idx, n_idx))
        } else {
            guess_compound_conditional(state, idx)
        };
    }

    // TODO: switch statements
    None
}

fn cyclic_region_type(state: &mut SPSState, idx: IndexType) -> Option<NodeType> {
    None
}

fn graph_substitute(state: &mut SPSState, repl_idx: IndexType, repl_tp: NodeType) {
    match &repl_tp {
        NodeType::Seq(v) => {
            state.reduce_graph.relink_in(*v.first().unwrap(), repl_idx);
            state.reduce_graph.relink_out(*v.last().unwrap(), repl_idx);
            for i in v {
                state.reduce_graph.detach(*i);
            }
        }
        NodeType::If(h, t) => {
            state.reduce_graph.relink_in(*h, repl_idx);
            state.reduce_graph.relink_out_multiple(&[*h, *t], repl_idx);

            state.reduce_graph.detach(*h);
            state.reduce_graph.detach(*t);
        }
        NodeType::IfInv(h, f) => {
            state.reduce_graph.relink_in(*h, repl_idx);
            state.reduce_graph.relink_out_multiple(&[*h, *f], repl_idx);

            state.reduce_graph.detach(*h);
            state.reduce_graph.detach(*f);
        }
        NodeType::IfElse(h, t, f) => {
            state.reduce_graph.relink_in(*h, repl_idx);
            state.reduce_graph.relink_out_multiple(&[*t, *f], repl_idx);

            state.reduce_graph.detach(*h);
            state.reduce_graph.detach(*t);
            state.reduce_graph.detach(*f);
        }
        NodeType::CompoundCond(v, _) => {
            state.reduce_graph.relink_in(*v.first().unwrap(), repl_idx);
            state.reduce_graph.relink_out_multiple(v, repl_idx);

            for i in v {
                state.reduce_graph.detach(*i);
            }
        }
        NodeType::Switch(h, v) => {
            state.reduce_graph.relink_in(*h, repl_idx);
            state.reduce_graph.relink_out_multiple(v, repl_idx);

            state.reduce_graph.detach(*h);
            for i in v {
                state.reduce_graph.detach(*i);
            }
        }
        NodeType::SelfLoop(b) => {
            state.reduce_graph.relink_in(*b, repl_idx);
            state.reduce_graph.relink_out(*b, repl_idx);

            state.reduce_graph.detach(*b);
        }
        NodeType::While(c, b) => {
            state.reduce_graph.relink_in_multiple(&[*c, *b], repl_idx);
            state.reduce_graph.relink_out_multiple(&[*c, *b], repl_idx);

            state.reduce_graph.detach(*c);
            state.reduce_graph.detach(*b);
        }
        NodeType::DoWhile(b, c) => {
            state.reduce_graph.relink_in_multiple(&[*b, *c], repl_idx);
            state.reduce_graph.relink_out_multiple(&[*b, *c], repl_idx);

            state.reduce_graph.detach(*b);
            state.reduce_graph.detach(*c);
        }
        NodeType::For(i, c, b) => {
            // preprocess cond and body nodes to allow easy relink
            state
                .reduce_graph
                .relink_in_multiple(&[*i, *c, *b], repl_idx);
            state
                .reduce_graph
                .relink_out_multiple(&[*i, *c, *b], repl_idx);

            state.reduce_graph.detach(*i);
            state.reduce_graph.detach(*c);
            state.reduce_graph.detach(*b);
        }
        NodeType::Goto(_) => unreachable!("Invalid NodeType::Goto produced by analysis"),
        NodeType::Tail => unreachable!("Invalid NodeType::Tail produced by analysis"),
        NodeType::Leaf(_) => unreachable!("Invalid NodeType::Leaf produced by analysis"),
        NodeType::Invalid => unreachable!("Invalid NodeType::Invalid produced by analysis"),
    }

    state
        .reduce_graph
        .vert_mut(repl_idx)
        .unwrap()
        .data_mut()
        .unwrap()
        .tp = repl_tp;
}

fn vec_keep_remove(
    vec: &mut Vec<IndexType>,
    repl: IndexType,
    keep: IndexType,
    mut rlist: impl Iterator<Item = IndexType>,
) -> IndexType {
    let mut last_replace: Option<IndexType> = Option::None;
    let mut ctr: IndexType = 0;
    vec.retain_mut(|elem| {
        let keep = if *elem == keep {
            *elem = repl;
            last_replace = Some(ctr);
            true
        } else if rlist.any(|relem| relem == *elem) {
            last_replace = Some(ctr);
            false
        } else {
            true
        };
        ctr += 1;
        keep
    });
    // Return the largest postorder number we replaced and continue from there
    last_replace.unwrap()
}

// Adjust the postorder list given the graph substitution about to be applied
fn adjust_postorder_for_substitution(
    repl_idx: IndexType,
    repl_tp: &NodeType,
    postorder: &mut Vec<IndexType>,
) -> IndexType {
    // Replace a list of nodes, given the first element of the list is the head
    match repl_tp {
        NodeType::Seq(v) => vec_keep_remove(postorder, repl_idx, v[0], v[1..].iter().copied()),
        NodeType::If(h, t) => vec_keep_remove(postorder, repl_idx, *h, [*t].iter().copied()),
        NodeType::IfInv(h, f) => vec_keep_remove(postorder, repl_idx, *h, [*f].iter().copied()),
        NodeType::IfElse(h, t, f) => {
            vec_keep_remove(postorder, repl_idx, *h, [*t, *f].iter().copied())
        }
        NodeType::CompoundCond(v, _) => {
            vec_keep_remove(postorder, repl_idx, v[0], v[1..].iter().copied())
        }
        NodeType::Switch(h, v) => vec_keep_remove(postorder, repl_idx, *h, v.iter().copied()),
        NodeType::SelfLoop(b) => vec_keep_remove(postorder, repl_idx, *b, std::iter::empty()),
        NodeType::While(c, b) => vec_keep_remove(postorder, repl_idx, *c, [*b].iter().copied()),
        NodeType::DoWhile(b, c) => vec_keep_remove(postorder, repl_idx, *b, [*c].iter().copied()),
        NodeType::For(i, c, b) => {
            vec_keep_remove(postorder, repl_idx, *i, [*c, *b].iter().copied())
        }

        NodeType::Goto(_) => unreachable!("Invalid NodeType::Goto produced by analysis"),
        NodeType::Tail => unreachable!("Invalid NodeType::Tail produced by analysis"),
        NodeType::Leaf(_) => unreachable!("Invalid NodeType::Leaf produced by analysis"),
        NodeType::Invalid => unreachable!("Invalid NodeType::Invalid produced by analysis"),
    }
}

pub fn semantic_preserving_structurize(flowgraph: &PDRoutine) -> Result<(), String> {
    let mut state = SPSState::new(flowgraph);

    while state.reduce_graph.size() > 1 {
        let mut postorder: Vec<IndexType> = Vec::new();
        {
            let mut post_it = state.reduce_graph.postorder_iter(true);
            while let Some(v) = post_it.next(&state.reduce_graph) {
                postorder.push(v);
            }
        }

        let mut i = 0;
        while i < postorder.len() && state.reduce_graph.size() > 1 {
            if let Some(tp) = acyclic_region_type(&mut state, postorder[i]) {
                let node_idx = state.add_worknode();
                i = adjust_postorder_for_substitution(node_idx, &tp, &mut postorder);
                graph_substitute(&mut state, node_idx, tp);
                continue;
            } else if let Some(tp) = cyclic_region_type(&mut state, postorder[i]) {
                let node_idx = state.add_worknode();
                i = adjust_postorder_for_substitution(node_idx, &tp, &mut postorder);
                graph_substitute(&mut state, node_idx, tp);
                continue;
            } else {
                // No valid region detected
                i += 1;
            }
        }
    }

    Err(String::from("Unimplemented"))
}
