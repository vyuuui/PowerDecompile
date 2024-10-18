use crate::ir::pdir::{IndexType, PDRoutine, PDExpression};

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

}

pub fn semantic_preserving_structurize(flowgraph: &PDRoutine) -> Result<ControlNode, String> {


    Err(String::from("Unimplemented"))
}
