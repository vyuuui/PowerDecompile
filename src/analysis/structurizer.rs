// COMPLETED: Postorder substitution, Graph substitution
// TO BE COMPLETED: Acyclic region detection, cyclic region detection, tail
// region detection, refinement, improved cyclic region detection

use crate::ir::pdir::PDRoutine;
use crate::util::graph::{Graph, IndexType};

pub trait Structurizer {
    // TODO: Result type?
    fn structurize(&mut self);
}

pub struct INode {
    tp: NodeType,
    // Index within graph
    gr_idx: IndexType,
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
    // Head, True, False
    IfElse(IndexType, IndexType, IndexType),
    // Counter-intuitively, this is an expression and not a structure, a design flaw?
    CompoundCond(Vec<IndexType>),
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

fn acyclic_region_type(state: &mut SPSState, idx: IndexType) -> Option<NodeType> {
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
        NodeType::IfElse(h, t, f) => {
            state.reduce_graph.relink_in(*h, repl_idx);
            state.reduce_graph.relink_out_multiple(&[*t, *f], repl_idx);

            state.reduce_graph.detach(*h);
            state.reduce_graph.detach(*t);
            state.reduce_graph.detach(*f);
        }
        NodeType::CompoundCond(v) => {
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
            state.reduce_graph.cut(*b, *b);
            state.reduce_graph.relink_in(*b, repl_idx);
            state.reduce_graph.relink_out(*b, repl_idx);

            state.reduce_graph.detach(*b);
        }
        NodeType::While(c, b) => {
            // preprocess cond and body nodes to allow easy relink
            state.reduce_graph.cut(*c, *b);
            state.reduce_graph.relink_in(*c, repl_idx);
            state.reduce_graph.relink_out(*c, repl_idx);

            state.reduce_graph.detach(*c);
            state.reduce_graph.detach(*b);
        }
        NodeType::DoWhile(b, c) => {
            // preprocess cond and body nodes to allow easy relink
            state.reduce_graph.cut(*b, *c);
            state.reduce_graph.relink_in(*b, repl_idx);
            state.reduce_graph.relink_out(*c, repl_idx);

            state.reduce_graph.detach(*b);
            state.reduce_graph.detach(*c);
        }
        NodeType::For(i, c, b) => {
            // preprocess cond and body nodes to allow easy relink
            state.reduce_graph.cut(*b, *c);
            state.reduce_graph.relink_in(*i, repl_idx);
            state.reduce_graph.relink_out(*c, repl_idx);

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
        NodeType::IfElse(h, t, f) => {
            vec_keep_remove(postorder, repl_idx, *h, [*t, *f].iter().copied())
        }
        NodeType::CompoundCond(v) => {
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
