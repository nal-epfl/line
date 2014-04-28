/*
Copyright 2008-2010 Gephi
Authors : Mathieu Bastian <mathieu.bastian@gephi.org>
Website : http://www.gephi.org

This file is part of Gephi.

Gephi is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as
published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

Gephi is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with Gephi.  If not, see <http://www.gnu.org/licenses/>.
*/
package org.gephi.toolkit.demos;

import java.io.File;
import java.io.IOException;
import java.util.Random;
import org.gephi.graph.api.Graph;
import org.gephi.graph.api.GraphController;
import org.gephi.graph.api.GraphModel;
import org.gephi.graph.api.Node;
import org.gephi.io.exporter.api.ExportController;
import org.gephi.io.exporter.spi.GraphExporter;
import org.gephi.io.importer.api.Container;
import org.gephi.io.importer.api.EdgeDefault;
import org.gephi.io.importer.api.ImportController;
import org.gephi.io.processor.plugin.DefaultProcessor;
import org.gephi.layout.plugin.force.StepDisplacement;
import org.gephi.layout.plugin.force.yifanHu.YifanHuLayout;
import org.gephi.layout.plugin.forceAtlas.ForceAtlasLayout;
import org.gephi.layout.plugin.openord.OpenOrdLayout;
import org.gephi.layout.plugin.random.RandomLayout;
import org.gephi.preview.api.PreviewController;
import org.gephi.project.api.ProjectController;
import org.gephi.project.api.Workspace;
import org.openide.util.Lookup;

/**
 * This demo shows several actions done with the toolkit, aiming to do a complete chain,
 * from data import to results.
 * <p>
 * This demo shows the following steps:
 * <ul><li>Create a project and a workspace, it is mandatory.</li>
 * <li>Import the <code>polblogs.gml</code> graph file in an import container.</li>
 * <li>Append the container to the main graph structure.</li>
 * <li>Filter the graph, using <code>DegreeFilter</code>.</li>
 * <li>Run layout manually.</li>
 * <li>Compute graph distance metrics.</li>
 * <li>Rank color by degree values.</li>
 * <li>Rank size by centrality values.</li>
 * <li>Configure preview to display labels and mutual edges differently.</li>
 * <li>Export graph as PDF.</li></ul>
 * 
 * @author Mathieu Bastian
 */
public class HeadlessSimple {

    public void script(String inputFileName, String outputFileName, String[] passes) {
        //Init a project - and therefore a workspace
        ProjectController pc = Lookup.getDefault().lookup(ProjectController.class);
        pc.newProject();
        Workspace workspace = pc.getCurrentWorkspace();

        //Get models and controllers for this new workspace - will be useful later
        GraphModel graphModel = Lookup.getDefault().lookup(GraphController.class).getModel();
        ImportController importController = Lookup.getDefault().lookup(ImportController.class);

        //Import file       
        Container container;
        
        try {
            File file = new File(inputFileName);
            container = importController.importFile(file);
            container.getLoader().setEdgeDefault(EdgeDefault.DIRECTED);   //Force DIRECTED
            container.setAutoScale(false);
        } catch (Exception ex) {
            ex.printStackTrace();
            return;
        }

        //Append imported data to GraphAPI
        importController.process(container, new DefaultProcessor(), workspace);

        //See if graph is well imported
        Graph graph = graphModel.getGraph();
        System.out.println("Nodes: " + graph.getNodeCount());
        System.out.println("Edges: " + graph.getEdgeCount());
        
        for (int i = 1; i <= graphModel.getGraph().getNodeCount(); i++) {
            Node n = graphModel.getGraph().getNode(i);
            System.out.println(n + " " + n.getNodeData().x() + " " + n.getNodeData().y() + " " + n.getNodeData().getSize());
        }
        
        for (String pass : passes) {
            if (pass.equals("Center")) {
                float xc, yc;
                xc = 0;
                yc = 0;
                for (int i = 1; i <= graph.getNodeCount(); i++) {
                    Node n = graph.getNode(i);
                    xc += n.getNodeData().x() / graph.getNodeCount();
                    yc += n.getNodeData().y() / graph.getNodeCount();
                }
                for (int i = 1; i <= graph.getNodeCount(); i++) {
                    Node n = graph.getNode(i);
                    n.getNodeData().setX(n.getNodeData().x() - xc);
                    n.getNodeData().setY(n.getNodeData().y() - yc);
                }
            } else if (pass.equals("Random")) {
                RandomLayout layout = new RandomLayout(null, 1000.0);
                layout.setGraphModel(graphModel);
                layout.initAlgo();
                layout.resetPropertiesValues();
                layout.goAlgo();
                layout.endAlgo();
            } else if (pass.equals("OpenOrd")) {
                OpenOrdLayout layout = new OpenOrdLayout(null);
                layout.resetPropertiesValues();
                layout.setGraphModel(graphModel);
                layout.initAlgo();
                System.out.println("OpenOrd: " +
                        layout.getLiquidStage() + " " +
                        layout.getExpansionStage() + " " +
                        layout.getCooldownStage() + " " + 
                        layout.getCrunchStage() + " " +
                        layout.getSimmerStage());
                while (layout.canAlgo()) {
                    layout.goAlgo();
                }
                layout.endAlgo();
            } else if (pass.startsWith("YifanHu")) {
                int maxIter = 100;
                String[] args = pass.split(":");    
                if (args.length > 1) {
                    maxIter = Integer.parseInt(args[1]);
                }
                YifanHuLayout layout = new YifanHuLayout(null, new StepDisplacement(1.0f));
                layout.setGraphModel(graphModel);
                layout.initAlgo();
                layout.resetPropertiesValues();
                for (int i = 0; i < maxIter && layout.canAlgo(); i++) {
                    layout.goAlgo();
                }
                layout.endAlgo();
            } else if (pass.startsWith("ForceAtlas")) {
                int maxIter = 100;
                String[] args = pass.split(":");
                if (args.length > 1) {
                    maxIter = Integer.parseInt(args[1]);
                }
                ForceAtlasLayout layout = new ForceAtlasLayout(null);
                layout.setGraphModel(graphModel);
                layout.initAlgo();
                layout.resetPropertiesValues();
                layout.setAdjustSizes(true);
                layout.setOutboundAttractionDistribution(true);
                layout.setRepulsionStrength(1000.0);
                layout.setAttractionStrength(10.0);
                layout.setGravity(10.0);
                layout.setMaxDisplacement(1000.0);
                for (int i = 0; i < maxIter && layout.canAlgo(); i++) {
                    layout.goAlgo();
                }
                layout.endAlgo();
            }
            
            for (int i = 1; i <= graphModel.getGraph().getNodeCount(); i++) {
                Node n = graphModel.getGraph().getNode(i);
                System.out.println(n + " " + n.getNodeData().x() + " " + n.getNodeData().y());
            }
            double xMean, yMean;
            xMean = yMean = 0;
            for (int i = 1; i <= graph.getNodeCount(); i++) {
                Node n = graph.getNode(i);
                System.out.println(n + " " + n.getNodeData().x() + " " + n.getNodeData().y());
                xMean += n.getNodeData().x() / graph.getNodeCount();
                yMean += n.getNodeData().y() / graph.getNodeCount();
            }
            double xVar, yVar;
            xVar = yVar = 0;
            for (int i = 1; i <= graph.getNodeCount(); i++) {
                Node n = graph.getNode(i);
                xVar += (n.getNodeData().x() - xMean)*(n.getNodeData().x() - xMean) / graph.getNodeCount();
                yVar += (n.getNodeData().y() - yMean)*(n.getNodeData().y() - yMean) / graph.getNodeCount();
            }
            if (xVar < 1 && yVar < 1) {
                RandomLayout layout = new RandomLayout(null, 1000.0);
                layout.setGraphModel(graphModel);
                layout.initAlgo();
                layout.resetPropertiesValues();
                layout.goAlgo();
                layout.endAlgo();
            }
        }
        
        //Export
        ExportController ec = Lookup.getDefault().lookup(ExportController.class);
        
        //Export only visible graph
        GraphExporter exporter = (GraphExporter) ec.getExporter("gexf");     //Get GEXF exporter
        exporter.setExportVisible(true);  //Only exports the visible (filtered) graph
        exporter.setWorkspace(workspace);
        try {
            ec.exportFile(new File(outputFileName), exporter);
        } catch (IOException ex) {
            ex.printStackTrace();
            return;
        }
    }
}
