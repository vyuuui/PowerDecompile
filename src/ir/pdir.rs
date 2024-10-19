use crate::util::graph::{Graph, IndexType};

pub struct PDExpression {
    // TODO: Fill
}

pub enum PDStatement {
    Assignment,
    Call,
    Intrinsic,
    FlowTransfer,
}

pub struct BasicBlock {
    pub statements: Vec<PDStatement>,
    pub transfer_cond: Option<PDExpression>,
}

pub struct PDRoutine {
    pub blocks: Graph<BasicBlock>,
}
