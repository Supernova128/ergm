% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/InitErgmProposal.R
\name{ConstantEdges-ergmProposal}
\alias{ConstantEdges-ergmProposal}
\alias{InitErgmProposal.ConstantEdges}
\title{MHp for edges constraints}
\description{
MHp for \eqn{constraints= ~edges}. Propose pairs of toggles that keep number of edges the same.
This is done by:
a. choosing an existing edge at random;
b. repeatedly choosing dyads at random until one is found that does not have an edge; and
c. proposing toggling both these dyads. Note that step b. will be very inefficient if the network is nearly
complete, so this proposal is NOT recommended for such networks. However, most network datasets are
sparse, so this is not likely to be an issue.
}
\details{
\if{html}{\Sexpr[results=rd,stage=render]{ergm:::.formatProposalsHtml(ergm:::.buildProposalsList(proposal="ConstantEdges"))}}
\if{text}{\Sexpr[results=rd,stage=render]{ergm:::.formatProposalsText(ergm:::.buildProposalsList(proposal="ConstantEdges"))}}
\if{latex}{\Sexpr[results=rd,stage=render]{ergm:::.formatProposalsLatex(ergm:::.buildProposalsList(proposal="ConstantEdges"))}}
}
\keyword{internal}
