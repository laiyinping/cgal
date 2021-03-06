#include <QApplication>
#include <QMainWindow>
#include <QMessageBox>
#include <QAction>
#include <QStringList>

#include "Scene_polyhedron_item.h"
#include "Polyhedron_type.h"

#include <CGAL/Three/Polyhedron_demo_plugin_interface.h>
#include <CGAL/Exact_rational.h>
#include <CGAL/Polyhedron_kernel.h>
#include <CGAL/convex_hull_3.h>
#include <CGAL/Dualizer.h>
#include <CGAL/translate.h>

#include "Kernel_type.h"
typedef Kernel::Triangle_3 Triangle;
typedef Kernel::Point_3 Point;
typedef Kernel::Vector_3 Vector;
typedef Kernel::Plane_3 Plane;
typedef Kernel::FT FT;
using namespace CGAL::Three;
class Polyhedron_demo_kernel_plugin : 
  public QObject,
  public Polyhedron_demo_plugin_interface
{
  Q_OBJECT
  Q_INTERFACES(CGAL::Three::Polyhedron_demo_plugin_interface)
  Q_PLUGIN_METADATA(IID "com.geometryfactory.PolyhedronDemo.PluginInterface/1.0")

public:
  QList<QAction*> actions() const {
    return _actions;
  }

  bool applicable(QAction*) const { 
    return qobject_cast<Scene_polyhedron_item*>(scene->item(scene->mainSelectionIndex()));
  }

   void init(QMainWindow* mainWindow, CGAL::Three::Scene_interface* scene_interface, Messages_interface*)
   {
     scene = scene_interface;
     mw = mainWindow;
     QAction* actionKernel = new QAction(tr("Kernel"), mainWindow);
     connect(actionKernel, SIGNAL(triggered()),
             this, SLOT(on_actionKernel_triggered()));
     _actions << actionKernel;
   }
public Q_SLOTS:
  void on_actionKernel_triggered();
private:
  QList<QAction*> _actions;
  CGAL::Three::Scene_interface* scene;
  QMainWindow* mw;

}; // end Polyhedron_demo_kernel_plugin


void Polyhedron_demo_kernel_plugin::on_actionKernel_triggered()
{
  const CGAL::Three::Scene_interface::Item_id index = scene->mainSelectionIndex();
  
  Scene_polyhedron_item* item = 
    qobject_cast<Scene_polyhedron_item*>(scene->item(index));

  if(item)
  {
    Polyhedron* pMesh = item->polyhedron();

    typedef CGAL::Exact_rational ET;
    typedef Polyhedron_kernel<Kernel,ET> Polyhedron_kernel;

    // get triangles from polyhedron
    std::list<Triangle> triangles;
    get_triangles(*pMesh,std::back_inserter(triangles));

    // solve LP 
    std::cout << "Solve linear program..." << triangles.size();
    Polyhedron_kernel kernel;
    if(!kernel.solve(triangles.begin(),triangles.end()))
    {
      std::cout << "done (empty kernel)" << std::endl;
      QMessageBox::information(mw, tr("Empty kernel"),
                               tr("The kernel of the polyhedron \"%1\" is empty.").
                               arg(item->name()));
      QApplication::restoreOverrideCursor();
      return;
    }
    std::cout << "done" << std::endl;

    // add kernel as new polyhedron
    Polyhedron *pKernel = new Polyhedron;

    // get inside point
    Point inside_point = kernel.inside_point();
    Vector translate = inside_point - CGAL::ORIGIN;

    // compute dual of translated polyhedron w.r.t. inside point.
    std::cout << "Compute dual of translated polyhedron...";
    std::list<Point> dual_points;
    std::list<Triangle>::iterator it;
    for(it = triangles.begin();
      it != triangles.end();
      it++)
    {
      const Triangle& triangle = *it;
      const Point p0 = triangle[0] - translate;
      const Point p1 = triangle[1] - translate;
      const Point p2 = triangle[2] - translate;
      Plane plane(p0,p1,p2); 
      Vector normal = plane.orthogonal_vector();
      normal = normal / std::sqrt(normal*normal);
      // compute distance to origin (do not use plane.d())
      FT distance_to_origin = std::sqrt(CGAL::squared_distance(Point(CGAL::ORIGIN),plane));
      Point dual_point = CGAL::ORIGIN + normal / distance_to_origin;
      dual_points.push_back(dual_point);
    }
    std::cout << "ok" << std::endl;

    // compute convex hull in dual space
    std::cout << "convex hull in dual space...";
    Polyhedron convex_hull;
    CGAL::convex_hull_3(dual_points.begin(),dual_points.end(),convex_hull);
    std::cout << "ok" << std::endl;

    // dualize and translate back to get final kernel
    Dualizer<Polyhedron,Kernel> dualizer;
    dualizer.run(convex_hull,*pKernel);
    ::translate<Polyhedron,Kernel>(*pKernel,translate);
    pKernel->inside_out();

    Scene_polyhedron_item* new_item = new Scene_polyhedron_item(pKernel);
    new_item->setName(tr("%1 (kernel)").arg(item->name()));
    new_item->setColor(Qt::magenta);
    new_item->setRenderingMode(item->renderingMode());
    item->setRenderingMode(Wireframe);

    scene->addItem(new_item);
    scene->itemChanged(item);

    QApplication::restoreOverrideCursor();
  }
}

#include "Kernel_plugin.moc"
