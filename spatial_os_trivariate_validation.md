# Spatial OS Design Validation: Trivariate Target Resolution

This document compiles the theoretical framework, mathematical validation, and spatial constraints for a non-perspective Spatial Operating System (OS). By using proxy volumes ("voodoo volumes") that map arbitrary sections of the far field into the user's near field, the OS eliminates the traditional perspective ray-casting bottlenecks found in typical XR interfaces.

---

## 1. Theoretical Framework & Formula Realignment

Traditional Fitts's Law fails in 3D space because it treats interfaces as flat 2D planes. To evaluate 3D target acquisition without ray-casting, we adapt the **Trivariate Pointing Model** pioneered by Grossman and Balakrishnan (CHI 2004). 

The original trivariate formula calculates the Index of Difficulty ($ID$) in bits based on target Width ($W$), Height ($H$), and Depth ($D$):

$$ID = \log_2 \left( \frac{A}{\min(W, H, D)} + 1 \right)$$

### The Symmetrical Voxel Override
Because proxy clusters can arrive in the near field at any arbitrary rotation, targets cannot be oblong or optimized for a single directional approach even though the Trivariate paper shows the depth vector is more error-prone. The system must design for the absolute lowest common denominator: the $Z$-axis. Therefore, the dimensions of a given empty chunk of interactable space should be a perfectly uniform cube where $W = H = D = S_{cube}$. 

The equation collapses into a single-variable constraint:

$$ID = \log_2 \left( \frac{A}{S_{cube}} + 1 \right)$$

To validate a target voxel size based on physical motor limits, we isolate $S_{cube}$:

$$S_{cube} = \frac{A}{2^{ID} - 1}$$

---

## 2. Input Parameters & Spatial Constraints

The near-field proxy interaction zone (voodoo volume) is defined as a bounding box anchored in the user's near-field motor space:

*   **Anchor Point:** Center point of the bottom plane manifests approximately 12 inches forward from the user's navel (estimated via head-pose offset).
*   **Bounding Box Dimensions:** 12" Wide $\times$ 12" Deep $\times$ 18" Tall (limited by safe coracobrachialis muscle range)

### Calculating Maximum Amplitude ($A$)
To establish a "bulletproof" voxel size that prevents overshooting during rapid hand gestures, we must evaluate the worst-case scenario: a maximum velocity diagonal sweep from one lower corner of the voodoo volume to the opposite upper corner.

$$\text{Diagonal} = \sqrt{12^2 + 12^2 + 18^2} = \sqrt{144 + 144 + 324} = \sqrt{612} \approx 24.74\text{ inches}$$

*   **Max Travel Distance ($A$):** **24.74 inches** (62.84 cm)
*   **Target Performance Threshold ($ID$):** **3.2 bits** (The empirical threshold where rapid, unimanual 3D hand movements can transition from ballistic motion to precise deceleration without overshoot errors or depth-axis drift according to the Trivariate paper's results).

---

## 3. Mathematical Validation & Core Recommendations

Plugging the physical constraints into the isolated symmetrical formula yields the following engineering requirements:

$$S_{cube} = \frac{24.74\text{ in}}{2^{3.2} - 1} = \frac{24.74\text{ in}}{9.19 - 1} = \frac{24.74\text{ in}}{8.19} \approx 3.02\text{ inches}$$

### Prime Voxel Size Recommendation
*   **Imperial Specification:** **3.0 inches**
*   **Metric Equivalent:** **7.6 cm**

A **3-inch symmetrical voxel** is the ideal lowest common denominator size that can accomodate rapid hand motion with minimal overshoot. Obviously, interactive elements can be smaller if the developer does not intend the user to dart between them rapidly, but 3" voxel targets is the min volume when designing for fastest hand motion. It ensures that a user can execute a blind, maximum-velocity swipe across the entire canvas, and their internal motor control loop will naturally halt their finger inside the target volume error-free.

* 3OS also supports easy creation of spherical interfaces which are similar to desktop radio button lists in that a list alternate OR choices are arranged near each other, but in 3OS they occupy equal angular space around a center point, and the hand's vector relative to that center point changes hover state and/selection of a given element. This interface style is the highest-perforcmance voxel-based multiple-choise option because it eliminates any possibility of overshoot.

### Resulting Matrix Resolution for the Voodoo Volume
Dividing the 12" $\times$ 12" $\times$ 18" bounding box by the 3-inch ideal voxel size yields a discrete, non-overlapping interaction matrix:

$$\text{Width Resolution} = \frac{12\text{"}}{3\text{"}} = 3\text{ voxels}$$
$$\text{Depth Resolution} = \frac{12\text{"}}{3\text{"}} = 3\text{ voxels}$$
$$\text{Height Resolution} = \frac{18\text{"}}{3\text{"}} = 6\text{ voxels}$$

> *Correction from baseline continuous volume approximations:* A strict grid alignment yields a **3 $\times$ 3 $\times$ 6 Matrix**, providing a total grid capacity of **54 independent, full-sized interaction voxels** cleanly packed into the voodoo volume.

---

## 4. Design Takeaways for High-Throughput Spatial Interaction

1.  **Zero-Overshoot Guarantee:** By capping the interactive resolution at a 3 $\times$ 3 $\times$ 6 grid within this physical volume, every single voxel functions as a "high-throughput" zone requiring zero training wheels.
2.  **Rotation Invariance:** Because every voxel is a 3-inch cube, the proxy volume remains completely stable and accurate regardless of whether the user is viewing the target cluster from the front, side, or back face.
3.  **Bypassing the Keyhole Effect:** While a single 3-inch target has lower micro-throughput than a 2-pixel desktop button, the macro-throughput of the spatial OS is drastically optimized by eliminating window management, ray-casting lag, and perspective flattening.
