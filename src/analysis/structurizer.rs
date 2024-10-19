use crate::ir::pdir::{BasicBlock, PDRoutine, PDExpression};
use crate::util::graph::{IndexType, Graph};

pub trait Structurizer {
    // TODO: Result type?
    fn structurize(&mut self);
}

pub enum ControlNode {
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
    Goto,
    Tail,
}

struct SPSState {
    rgraph: Graph<ControlNode>,
}

// TODO: This is unnecessarily slow
fn init_analysis_graph(graph: &Graph<BasicBlock>) -> Graph<ControlNode> {
    let mut copy_graph = Graph::default();
    for (idx, v) in graph.vert_iter().enumerate() {
        if v.is_pseudo() {
            continue;
        }
        // TODO: Get source index from node instead here
        copy_graph.add_vert(ControlNode::Leaf(idx));
    }
    for (from, v) in graph.vert_iter().enumerate() {
        for to in v.edge_iter(true) {
            copy_graph.link(from, to);
        }
    }

    copy_graph
}

pub fn semantic_preserving_structurize(flowgraph: &PDRoutine) -> Result<ControlNode, String> {
    let mut state = SPSState { rgraph: init_analysis_graph(&flowgraph.blocks) };


    Err(String::from("Unimplemented"))
}
